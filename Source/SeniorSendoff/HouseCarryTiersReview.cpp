#include "HouseCarryTiersReview.h"
#include "HouseExterior.h"
#include "HouseTreeReaction.h"
#include "HouseBlackHall.h"

#if !UE_BUILD_SHIPPING
#include "HouseFurnitureInteractionComponent.h"
#include "HouseInteractiveProp.h"
#include "SeniorLobby.h"
#include "StoryCampaign.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
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
#include "StoryMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HouseBreakDebris.h"
#include "HouseClothFlag.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/DamageType.h"

namespace
{
constexpr float TierFloorZ=8000.f;
const FVector TierOrigin(0,9000,TierFloorZ);

struct FTierReviewState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AHouseInteractiveProp> Prop;
    TArray<TWeakObjectPtr<AActor>> Transients;
    TSharedPtr<FJsonObject> Report, TierReport;
    TArray<TSharedPtr<FJsonValue>> Tiers, Captures;
    FString Evidence;
    int32 Tier=0, Step=0, LastStep=-1;
    double Started=0, StepStarted=0, WallStarted=0;
    FVector WalkStart=FVector::ZeroVector, ThrowStart=FVector::ZeroVector, GripStart=FVector::ZeroVector;
    FRotator PropStartRotation=FRotator::ZeroRotator, InitialAim=FRotator::ZeroRotator;
    float PeakSpeed=0.f, MaxLift=0.f, MaxTilt=0.f, WorstHand=0.f, WorstTarget=0.f, WorstCamToGrip=0.f;
    TSet<FString> Taken;
    bool bSetup=false;
};

const TCHAR* TierName(int32 Tier){ return Tier==0?TEXT("small"):(Tier==1?TEXT("medium"):TEXT("large")); }

void TierCapture(FTierReviewState& S,const FString& Name)
{
    if(S.Taken.Contains(Name) || FScreenshotRequest::IsScreenshotRequested())return;
    S.Taken.Add(Name);
    const FString Path=FPaths::Combine(S.Evidence,Name+TEXT(".png"));
    FScreenshotRequest::RequestScreenshot(Path,false,false);
    S.Captures.Add(MakeShared<FJsonValueString>(Path));
}

void TierAim(AStoryFirstPersonCharacter* Pawn,APlayerController* PC,const FVector& Point)
{
    PC->SetControlRotation((Point-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
    if(PC->PlayerCameraManager)PC->PlayerCameraManager->UpdateCamera(0.f);
}

FString GTierReferenceOverride;
AHouseInteractiveProp* FindTierReference(UWorld* World,int32 Tier)
{
    // Prefer the most common real pieces in the house for each size.
    const FString PreferredText=!GTierReferenceOverride.IsEmpty()?GTierReferenceOverride
        :FString(Tier==0?TEXT("throwpillow"):(Tier==1?TEXT("greenspindlechair"):TEXT("reclinersofa")));
    const TCHAR* Preferred=*PreferredText;
    for(TActorIterator<AHouseInteractiveProp> It(World);It;++It)
        if(It->IntactMesh && It->Body && !It->bBroken && It->CanBeMoved()
            && (It->PropName+It->PropId.ToString()).ToLower().Contains(Preferred))return *It;
    for(TActorIterator<AHouseInteractiveProp> It(World);It;++It)
    {
        if(!It->IntactMesh || !It->Body || It->bBroken || !It->CanBeMoved() || It->bWallMounted)continue;
        const FString Name=(It->PropName+TEXT(" ")+It->PropId.ToString()).ToLower();
        const FVector Size=It->Body->Bounds.BoxExtent*2.f;
        const float Wide=FMath::Max(Size.X,Size.Y);
        if(Tier==0 && (Name.Contains(TEXT("pillow")) || Name.Contains(TEXT("cushion"))) && Size.GetMax()>=20.f && Size.GetMax()<=75.f)
            return *It;
        if(Tier==1 && (Name.Contains(TEXT("chair")) || Name.Contains(TEXT("ottoman")) || Name.Contains(TEXT("stool")))
            && !Name.Contains(TEXT("arm")) && Wide>=35.f && Wide<=110.f && Size.Z>=35.f && Size.Z<=120.f)
            return *It;
        if(Tier==2 && (Name.Contains(TEXT("sofa")) || Name.Contains(TEXT("couch"))) && !Name.Contains(TEXT("remote"))
            && Wide>120.f && Wide<320.f && Size.Z>40.f && Size.Z<150.f)
            return *It;
    }
    return nullptr;
}

AHouseInteractiveProp* SpawnTierProp(FTierReviewState& S,UWorld* World,int32 Tier)
{
    AHouseInteractiveProp* Reference=FindTierReference(World,Tier);
    const FVector FallbackScale=Tier==0?FVector(.4f,.3f,.15f):(Tier==1?FVector(.55f,.5f,.8f):FVector(2.f,.9f,.8f));
    const FTransform Transform(Reference?Reference->GetActorRotation():FRotator::ZeroRotator,TierOrigin+FVector(150,0,100),
        Reference?Reference->GetActorScale3D():FallbackScale);
    AHouseInteractiveProp* Prop=World->SpawnActorDeferred<AHouseInteractiveProp>(AHouseInteractiveProp::StaticClass(),Transform,
        nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if(!Prop)return nullptr;
    Prop->SetFlags(RF_Transient);
    Prop->IntactMesh=Reference?Reference->IntactMesh.Get():LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    Prop->PropName=FString::Printf(TEXT("tier review %s"),TierName(Tier));
    Prop->PropId=*FString::Printf(TEXT("TierReview_%s"),TierName(Tier));
    Prop->MassKg=Tier==0?3.f:(Tier==1?16.f:120.f);
    Prop->CarrySize=Tier==0?ESSOCarrySize::Small:(Tier==1?ESSOCarrySize::Medium:ESSOCarrySize::Large);
    Prop->bMovable=true;Prop->bBreakable=false;Prop->bWallMounted=false;
    if(Reference)
        for(int32 Slot=0;Slot<Reference->Body->GetNumMaterials();++Slot)Prop->MaterialOverrides.Add(Reference->Body->GetMaterial(Slot));
    Prop->FinishSpawning(Transform);
    S.Transients.Add(Prop);
    const FBox Box=Prop->Body->Bounds.GetBox();
    const FVector Delta(TierOrigin.X+(Tier==2?95.f:80.f)-Box.Min.X,TierOrigin.Y-Prop->Body->Bounds.Origin.Y,TierFloorZ+1.f-Box.Min.Z);
    Prop->SetActorLocation(Prop->GetActorLocation()+Delta,false,nullptr,ETeleportType::TeleportPhysics);
    Prop->WakeForInteraction();
    S.TierReport->SetStringField(TEXT("source_prop"),Reference?Reference->PropId.ToString():TEXT("engine_cube_fallback"));
    S.TierReport->SetStringField(TEXT("mesh"),GetNameSafe(Prop->IntactMesh));
    S.TierReport->SetStringField(TEXT("size_cm"),(Prop->Body->Bounds.BoxExtent*2.f).ToString());
    return Prop;
}

float HandError(AStoryFirstPersonCharacter* Pawn,UHouseFurnitureInteractionComponent* Mover)
{
    FVector C,N;float Worst=0.f;
    for(bool Right:{true,false})
    {
        if(!Right && !Mover->UsesTwoHands())continue;
        Mover->GetHandContact(Right,C,N);
        Worst=FMath::Max(Worst,float(FVector::Dist(C,Pawn->GetFurnitureHandGripPoint(Right))));
    }
    return Worst;
}
}


namespace
{
struct FPushReviewState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AHouseInteractiveProp> Prop;
    TSharedPtr<FJsonObject> Report;
    TArray<TSharedPtr<FJsonValue>> Runs;
    int32 Run=0, Step=0;
    double StepStarted=0, WallStarted=0;
    FVector Start=FVector::ZeroVector; FQuat StartRot=FQuat::Identity; float StartBottom=0.f;
    float MaxSpeed=0.f, MaxRise=0.f, MaxSpin=0.f, MaxTilt=0.f, PawnSpeedAtHit=0.f, MaxLean=0.f;
};
}

void TickHouseFurniturePushReview(AStoryFirstPersonCharacter* Pawn)
{
    if(!Pawn->HasAuthority() || !Pawn->IsLocallyControlled())return;
    UWorld* World=Pawn->GetWorld();
    if(!World || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    APlayerController* PC=Cast<APlayerController>(Pawn->GetController());
    UStoryMovementComponent* Move=Cast<UStoryMovementComponent>(Pawn->GetCharacterMovement());
    if(!PC || !Move || !Pawn->FirstPersonCamera)return;
    static FPushReviewState P; static FTierReviewState Holder;
    const double Now=World->GetTimeSeconds();
    if(P.World.Get()!=World){P=FPushReviewState();P.World=World;
        if(FParse::Param(FCommandLine::Get(),TEXT("OfficeChairsOnly")))P.Run=5;P.StepStarted=Now;P.WallStarted=FPlatformTime::Seconds();P.Report=MakeShared<FJsonObject>();
        Holder=FTierReviewState();Holder.World=World;Holder.Report=MakeShared<FJsonObject>();
        Holder.Evidence=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("HouseFurniturePushReview")));
        IFileManager::Get().MakeDirectory(*Holder.Evidence,true);}
    if(P.Step<0)return;
    const float Age=Now-P.StepStarted;
    auto Next=[&](int32 S){P.Step=S;P.StepStarted=Now;};
    // Runs: 0 large/engine, 1 medium/engine, 2 large/fixed, 3 medium/fixed, 4 small/fixed,
    // 5 office chair sprint, 6 office chair walk, 7 office chair off-centre sprint
    const bool bOffice=P.Run>=5;
    const int32 Tier=bOffice?1:(P.Run==4?0:(P.Run%2==0?2:1));
    const int32 Fix=P.Run>=2?1:0;
    GTierReferenceOverride=bOffice?TEXT("meshofficechair"):TEXT("");
    if(P.Run>7 || FPlatformTime::Seconds()-P.WallStarted>160.)
    {
        P.Report->SetArrayField(TEXT("runs"),P.Runs);
        FString Json;FJsonSerializer::Serialize(P.Report.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json,*FPaths::Combine(Holder.Evidence,TEXT("result.json")));
        P.Step=-1;FPlatformMisc::RequestExitWithStatus(false,0);return;
    }
    AHouseInteractiveProp* Prop=P.Prop.Get();
    if(P.Step==0)
    {
        if(Age<2.f)return;
        FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if(AStaticMeshActor* Floor=World->SpawnActor<AStaticMeshActor>(TierOrigin-FVector(0,0,20),FRotator::ZeroRotator,Params))
        {
            Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
            Floor->SetActorScale3D(FVector(80,80,.4f));
            Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
            if(UMaterialInterface* FloorMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial")))
                Floor->GetStaticMeshComponent()->SetMaterial(0,FloorMaterial);
        }
        for(const FVector& Offset:{FVector(60,-220,300),FVector(80,220,320),FVector(-150,0,350),FVector(300,0,250)})
            if(APointLight* Lamp=World->SpawnActor<APointLight>(TierOrigin+Offset,FRotator::ZeroRotator,Params))
            {
                UPointLightComponent* Bulb=Cast<UPointLightComponent>(Lamp->GetLightComponent());
                Bulb->SetMobility(EComponentMobility::Movable);Bulb->SetIntensityUnits(ELightUnits::Lumens);
                Bulb->SetIntensity(400000.f);Bulb->SetAttenuationRadius(3000.f);Bulb->SetCastShadows(true);
            }
        FPostProcessSettings& Exposure=Pawn->FirstPersonCamera->PostProcessSettings;
        Exposure.bOverride_AutoExposureMethod=true;Exposure.AutoExposureMethod=AEM_Basic;
        Exposure.bOverride_AutoExposureMinBrightness=true;Exposure.AutoExposureMinBrightness=2.f;
        Exposure.bOverride_AutoExposureMaxBrightness=true;Exposure.AutoExposureMaxBrightness=2.f;
        Pawn->FirstPersonCamera->PostProcessBlendWeight=1.f;
        Next(1);return;
    }
    if(P.Step==1)
    {
        if(IConsoleVariable* Var=IConsoleManager::Get().FindConsoleVariable(TEXT("sso.FurniturePushFix")))Var->Set(Fix);
        Holder.TierReport=MakeShared<FJsonObject>();
        Holder.TierReport->SetStringField(TEXT("tier"),TierName(Tier));
        Holder.TierReport->SetNumberField(TEXT("fix"),Fix);
        Holder.TierReport->SetStringField(TEXT("case"),P.Run==5?TEXT("office_sprint"):P.Run==6?TEXT("office_walk"):P.Run==7?TEXT("office_offcentre_sprint"):TEXT("furniture"));
        Prop=SpawnTierProp(Holder,World,Tier);P.Prop=Prop;
        Move->StopMovementImmediately();Move->SetMovementMode(MOVE_Walking);Move->SetSprintHeld(false);
        Pawn->SetActorLocation(TierOrigin+FVector(-320,P.Run==7?22.f:0.f,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f),false,nullptr,ETeleportType::TeleportPhysics);
        PC->SetControlRotation(FRotator::ZeroRotator);
        P.MaxSpeed=P.MaxRise=P.MaxSpin=P.MaxTilt=P.PawnSpeedAtHit=P.MaxLean=0.f;
        Next(2);return;
    }
    if(!Prop){++P.Run;Next(1);return;}
    if(P.Step==2)
    {
        if(Age<1.f)return;
        P.Start=Prop->GetActorLocation();P.StartRot=Prop->GetActorQuat();P.StartBottom=Prop->Body->Bounds.GetBox().Min.Z;
        Holder.TierReport->SetNumberField(TEXT("mass_kg"),Prop->Body->GetMass());
        Next(3);return;
    }
    auto Sample=[&]()
    {
        P.MaxSpeed=FMath::Max(P.MaxSpeed,float(Prop->Body->GetPhysicsLinearVelocity().Size()));
        P.MaxRise=FMath::Max(P.MaxRise,float(Prop->Body->Bounds.GetBox().Min.Z-P.StartBottom));
        P.MaxSpin=FMath::Max(P.MaxSpin,float(Prop->Body->GetPhysicsAngularVelocityInDegrees().Size()));
        P.MaxTilt=FMath::Max(P.MaxTilt,float(FMath::RadiansToDegrees((Prop->GetActorQuat()*P.StartRot.Inverse()).GetAngle())));
        // Lean from upright only (ignores swivel).
        P.MaxLean=FMath::Max(P.MaxLean,float(FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Prop->GetActorUpVector(),P.StartRot.GetUpVector()),-1.f,1.f)))));
    };
    if(P.Step==3)
    {
        // Sprint straight into the piece and keep running against it.
        Move->SetSprintHeld(P.Run!=6);
        // Walk/sprint into it for a moment, then stop and watch it coast.
        if(Age<(bOffice?1.05f:2.4f))Pawn->AddMovementInput(FVector::ForwardVector,1.f);
        P.PawnSpeedAtHit=FMath::Max(P.PawnSpeedAtHit,float(Pawn->GetVelocity().Size2D()));
        Sample();
        if(Age>1.f)TierCapture(Holder,FString::Printf(TEXT("%s_fix%d_pushing"),TierName(Tier),Fix));
        if(Age<2.4f)return;
        Move->SetSprintHeld(false);
        Next(4);return;
    }
    if(P.Step==4)
    {
        Sample();
        if(bOffice && Age>.4f)TierCapture(Holder,FString::Printf(TEXT("office_run%d_coasting"),P.Run));
        if(Age<(bOffice?3.f:1.2f))return;
        Holder.TierReport->SetNumberField(TEXT("player_peak_speed_cm_s"),P.PawnSpeedAtHit);
        Holder.TierReport->SetNumberField(TEXT("prop_peak_speed_cm_s"),P.MaxSpeed);
        Holder.TierReport->SetNumberField(TEXT("prop_peak_rise_cm"),P.MaxRise);
        Holder.TierReport->SetNumberField(TEXT("prop_peak_spin_deg_s"),P.MaxSpin);
        Holder.TierReport->SetNumberField(TEXT("prop_max_tilt_deg"),P.MaxTilt);
        Holder.TierReport->SetNumberField(TEXT("prop_max_lean_deg"),P.MaxLean);
        Holder.TierReport->SetNumberField(TEXT("prop_final_lean_deg"),FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Prop->GetActorUpVector(),P.StartRot.GetUpVector()),-1.f,1.f))));
        Holder.TierReport->SetNumberField(TEXT("prop_moved_cm"),FVector::Dist2D(Prop->GetActorLocation(),P.Start));
        Holder.TierReport->SetNumberField(TEXT("prop_yaw_change_deg"),FMath::Abs(FRotator::NormalizeAxis(Prop->GetActorRotation().Yaw-P.StartRot.Rotator().Yaw)));
        Holder.TierReport->SetBoolField(TEXT("rolling_chair"),Prop->IsRollingChair());
        P.Runs.Add(MakeShared<FJsonValueObject>(Holder.TierReport));
        Prop->Destroy();
        ++P.Run;Next(1);return;
    }
}


namespace
{
struct FBreakReviewState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AHouseInteractiveProp> Prop;
    TArray<TSharedPtr<FJsonValue>> Runs;
    int32 Run=0, Step=0;
    double StepStarted=0, WallStarted=0;
    FVector Start=FVector::ZeroVector; float StartBottom=0.f, PeakSpeed=0.f, PeakRise=0.f, PeakSpin=0.f; int32 PeakLive=0;
};
const TCHAR* const BreakTargets[]={TEXT("greenspindlechair"),TEXT("reclinersofa"),TEXT("ovalcoffeetable"),TEXT("throwpillow"),TEXT("framedcollage")};
const TCHAR* const GreatRoomCouchTargets[]={TEXT("sm_gr3_nearseating__001"),TEXT("sm_gr3_nearseating__003"),
    TEXT("sm_gr3_farseating__001"),TEXT("sm_gr3_farseating__002"),TEXT("sm_gr3_farseating__003")};
}

void TickHouseBreakReview(AStoryFirstPersonCharacter* Pawn)
{
    if(!Pawn->HasAuthority() || !Pawn->IsLocallyControlled())return;
    UWorld* World=Pawn->GetWorld();
    if(!World || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    const bool bGreatRoomCouches=FParse::Param(FCommandLine::Get(),TEXT("HouseGreatRoomCushionReview"));
    APlayerController* PC=Cast<APlayerController>(Pawn->GetController());
    if(!PC || !Pawn->FirstPersonCamera)return;
    static FBreakReviewState B; static FTierReviewState Holder;
    const double Now=World->GetTimeSeconds();
    if(B.World.Get()!=World)
    {
        B=FBreakReviewState();B.World=World;B.StepStarted=Now;B.WallStarted=FPlatformTime::Seconds();
        Holder=FTierReviewState();Holder.World=World;Holder.Report=MakeShared<FJsonObject>();
        Holder.Evidence=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),
            bGreatRoomCouches?TEXT("HouseGreatRoomCushionReview"):TEXT("HouseBreakReview")));
        IFileManager::Get().MakeDirectory(*Holder.Evidence,true);
    }
    if(B.Step<0)return;
    const float Age=Now-B.StepStarted;
    auto Next=[&](int32 S){B.Step=S;B.StepStarted=Now;};
    constexpr int32 RunCount=UE_ARRAY_COUNT(BreakTargets);
    if(B.Run>=RunCount || FPlatformTime::Seconds()-B.WallStarted>150.)
    {
        if(FScreenshotRequest::IsScreenshotRequested() && Age<3.f)return;
        TSharedPtr<FJsonObject> Report=MakeShared<FJsonObject>();
        Report->SetArrayField(TEXT("runs"),B.Runs);
        FString Json;FJsonSerializer::Serialize(Report.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json,*FPaths::Combine(Holder.Evidence,TEXT("result.json")));
        B.Step=-1;FPlatformMisc::RequestExitWithStatus(false,0);return;
    }
    const FString Name=FString(bGreatRoomCouches?GreatRoomCouchTargets[B.Run]:BreakTargets[B.Run]);
    AHouseInteractiveProp* Prop=B.Prop.Get();
    if(B.Step==0)
    {
        if(Age<2.f)return;
        if(ASeniorLobbyPlayerState* Selection=Pawn->GetPlayerState<ASeniorLobbyPlayerState>()){Selection->CharacterIndex=0;Pawn->ApplySelectedCharacter();}
        FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if(AStaticMeshActor* Floor=World->SpawnActor<AStaticMeshActor>(TierOrigin-FVector(0,0,20),FRotator::ZeroRotator,Params))
        {
            Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
            Floor->SetActorScale3D(FVector(80,80,.4f));
            Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
            if(UMaterialInterface* FloorMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial")))
                Floor->GetStaticMeshComponent()->SetMaterial(0,FloorMaterial);
        }
        for(const FVector& Offset:{FVector(60,-220,300),FVector(80,220,320),FVector(-150,0,350),FVector(300,0,250)})
            if(APointLight* Lamp=World->SpawnActor<APointLight>(TierOrigin+Offset,FRotator::ZeroRotator,Params))
            {
                UPointLightComponent* Bulb=Cast<UPointLightComponent>(Lamp->GetLightComponent());
                Bulb->SetMobility(EComponentMobility::Movable);Bulb->SetIntensityUnits(ELightUnits::Lumens);
                Bulb->SetIntensity(400000.f);Bulb->SetAttenuationRadius(3000.f);Bulb->SetCastShadows(true);
            }
        FPostProcessSettings& Exposure=Pawn->FirstPersonCamera->PostProcessSettings;
        Exposure.bOverride_AutoExposureMethod=true;Exposure.AutoExposureMethod=AEM_Basic;
        Exposure.bOverride_AutoExposureMinBrightness=true;Exposure.AutoExposureMinBrightness=2.f;
        Exposure.bOverride_AutoExposureMaxBrightness=true;Exposure.AutoExposureMaxBrightness=2.f;
        Pawn->FirstPersonCamera->PostProcessBlendWeight=1.f;
        Next(1);return;
    }
    if(B.Step==1)
    {
        AHouseInteractiveProp* Reference=nullptr;
        for(TActorIterator<AHouseInteractiveProp> It(World);It;++It)
            if(It->IntactMesh && It->BrokenMesh && (It->PropName+It->PropId.ToString()).ToLower().Contains(Name)){Reference=*It;break;}
        Holder.TierReport=MakeShared<FJsonObject>();
        Holder.TierReport->SetStringField(TEXT("target"),Name);
        if(!Reference){Holder.TierReport->SetStringField(TEXT("error"),TEXT("no reference prop with broken mesh"));B.Runs.Add(MakeShared<FJsonValueObject>(Holder.TierReport));++B.Run;return;}
        const FTransform Transform(FRotator(0,Reference->GetActorRotation().Yaw,0),TierOrigin+FVector(170,0,120),Reference->GetActorScale3D());
        Prop=World->SpawnActorDeferred<AHouseInteractiveProp>(AHouseInteractiveProp::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        Prop->SetFlags(RF_Transient);
        Prop->IntactMesh=Reference->IntactMesh;Prop->DamagedMesh=Reference->DamagedMesh;
        Prop->HeavilyDamagedMesh=Reference->HeavilyDamagedMesh;Prop->BrokenMesh=Reference->BrokenMesh;
        Prop->MaterialOverrides=Reference->MaterialOverrides;
        Prop->MassKg=Reference->MassKg;Prop->BreakHealth=Reference->BreakHealth;Prop->DamageCategory=Reference->DamageCategory;
        Prop->PropName=Reference->PropName;Prop->PropId=*FString::Printf(TEXT("BreakReview_%s"),*Name);
        Prop->bMovable=true;Prop->bBreakable=true;Prop->bWallMounted=false;Prop->bBrokenVersionMovable=true;
        for(const FName& Tag:Reference->Tags)if(Tag==TEXT("HouseWallTelevision"))Prop->Tags.Add(Tag);
        Prop->FinishSpawning(Transform);
        const FBox Box=Prop->Body->Bounds.GetBox();
        Prop->SetActorLocation(Prop->GetActorLocation()+FVector(0,TierOrigin.Y-Box.GetCenter().Y,TierFloorZ+1.f-Box.Min.Z),false,nullptr,ETeleportType::TeleportPhysics);
        Prop->WakeForInteraction();
        B.Prop=Prop;
        Holder.TierReport->SetStringField(TEXT("source_prop"),Reference->PropId.ToString());
        Holder.TierReport->SetNumberField(TEXT("mass_kg"),Reference->MassKg);
        Holder.TierReport->SetNumberField(TEXT("break_health"),Reference->BreakHealth);
        Holder.TierReport->SetStringField(TEXT("size_cm"),(Prop->Body->Bounds.BoxExtent*2.f).ToString());
        auto* Move=Pawn->GetCharacterMovement();Move->StopMovementImmediately();Move->SetMovementMode(MOVE_Walking);
        // Stand back far enough to see the whole piece and its debris.
        const float Back=FMath::Clamp(Prop->Body->Bounds.BoxExtent.GetMax()*1.6f+90.f,150.f,420.f);
        Pawn->SetActorLocation(FVector(Prop->Body->Bounds.Origin.X-Back,TierOrigin.Y,TierFloorZ+Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f),false,nullptr,ETeleportType::TeleportPhysics);
        B.PeakSpeed=B.PeakRise=B.PeakSpin=0.f;B.PeakLive=0;
        Next(2);return;
    }
    if(!Prop){++B.Run;Next(1);return;}
    TierAim(Pawn,PC,Prop->Body->Bounds.Origin-FVector(0,0,Prop->Body->Bounds.BoxExtent.Z*.2f));
    auto Hit=[&](float Amount)
    {
        FHitResult HitInfo;
        const FVector From=Pawn->FirstPersonCamera->GetComponentLocation();
        FCollisionQueryParams Query(SCENE_QUERY_STAT(BreakReviewHit),true,Pawn);
        if(!Prop->Body->LineTraceComponent(HitInfo,From,Prop->Body->Bounds.Origin+(Prop->Body->Bounds.Origin-From).GetSafeNormal()*200.f,Query))
        { HitInfo.ImpactPoint=Prop->Body->Bounds.Origin; HitInfo.ImpactNormal=-FVector::ForwardVector; }
        HitInfo.Location=HitInfo.ImpactPoint;HitInfo.Component=Prop->Body;HitInfo.HitObjectHandle=FActorInstanceHandle(Prop);
        UGameplayStatics::ApplyPointDamage(Prop,Amount,(HitInfo.ImpactPoint-From).GetSafeNormal(),HitInfo,PC,Pawn,UDamageType::StaticClass());
    };
    auto Sample=[&]()
    {
        B.PeakSpeed=FMath::Max(B.PeakSpeed,float(Prop->Body->GetPhysicsLinearVelocity().Size()));
        B.PeakRise=FMath::Max(B.PeakRise,float(Prop->Body->Bounds.GetBox().Min.Z-B.StartBottom));
        B.PeakSpin=FMath::Max(B.PeakSpin,float(Prop->Body->GetPhysicsAngularVelocityInDegrees().Size()));
        if(Prop->BreakDebris)B.PeakLive=FMath::Max(B.PeakLive,Prop->BreakDebris->GetLivePieceCount());
    };
    const float Third=Prop->BreakHealth/3.f+.05f;
    if(B.Step==2){ if(Age<1.2f)return; TierCapture(Holder,Name+TEXT("_0_intact")); B.Start=Prop->GetActorLocation(); B.StartBottom=Prop->Body->Bounds.GetBox().Min.Z; Next(3); return; }
    if(B.Step==3){ if(Age<.4f)return; Hit(Third); Holder.TierReport->SetNumberField(TEXT("stage_after_hit1"),Prop->DamageStage); Next(4); return; }
    if(B.Step==4){ Sample(); if(Age>.07f)TierCapture(Holder,Name+TEXT("_1_crack1")); if(Age<1.2f)return; Hit(Third); Holder.TierReport->SetNumberField(TEXT("stage_after_hit2"),Prop->DamageStage); Next(5); return; }
    if(B.Step==5){ Sample(); if(Age>.07f)TierCapture(Holder,Name+TEXT("_2_crack2")); if(Age<1.2f)return;
        B.PeakSpeed=B.PeakRise=B.PeakSpin=0.f;B.StartBottom=Prop->Body->Bounds.GetBox().Min.Z;
        Hit(Third*1.5f); Holder.TierReport->SetBoolField(TEXT("broken"),Prop->bBroken); Next(6); return; }
    if(B.Step==6)
    {
        Sample();
        if(Age>.05f)TierCapture(Holder,Name+TEXT("_3_break_005"));
        if(Age>.22f)TierCapture(Holder,Name+TEXT("_4_break_022"));
        if(Age>.6f)TierCapture(Holder,Name+TEXT("_5_break_060"));
        if(Age>2.5f)TierCapture(Holder,Name+TEXT("_6_settled"));
        if(Age<2.8f)return;
        Holder.TierReport->SetNumberField(TEXT("debris_spawned"),Prop->BreakDebris?Prop->BreakDebris->GetSpawnedPieceCount():0);
        Holder.TierReport->SetNumberField(TEXT("debris_peak_live"),B.PeakLive);
        Holder.TierReport->SetNumberField(TEXT("break_peak_speed_cm_s"),B.PeakSpeed);
        Holder.TierReport->SetNumberField(TEXT("break_peak_rise_cm"),B.PeakRise);
        Holder.TierReport->SetNumberField(TEXT("break_peak_spin_deg_s"),B.PeakSpin);
        Holder.TierReport->SetNumberField(TEXT("moved_cm"),FVector::Dist2D(Prop->GetActorLocation(),B.Start));
        Holder.TierReport->SetStringField(TEXT("final_mesh"),GetNameSafe(Prop->Body->GetStaticMesh()));
        B.Runs.Add(MakeShared<FJsonValueObject>(Holder.TierReport));
        Prop->Destroy();
        ++B.Run;Next(1);return;
    }
}

namespace
{
struct FRoomCushionReviewState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AHouseInteractiveProp> Sofa;
    TArray<TWeakObjectPtr<AHouseInteractiveProp>> Pillows;
    TArray<FVector> PillowStarts;
    TArray<float> PeakSpeeds;
    TArray<float> PeakRises;
    TArray<TSharedPtr<FJsonValue>> Runs;
    FString Evidence;
    int32 SofaIndex=0, Step=0;
    double StepStarted=0;
};
const TCHAR* const RoomSofas[]={TEXT("SM_GR3_NearSeating__003"),TEXT("SM_GR3_NearSeating__001")};
const TCHAR* const RoomPillows[][2]={
    {TEXT("SM_GR3_NearSeating__004"),TEXT("SM_GR3_NearSeating__005")},
    {TEXT("SM_GR3_NearSeating__006"),TEXT("SM_GR3_NearSeating__007")}
};
}

void TickHouseGreatRoomPillowReview(AStoryFirstPersonCharacter* Pawn)
{
    if(!Pawn || !Pawn->HasAuthority() || !Pawn->IsLocallyControlled())return;
    UWorld* World=Pawn->GetWorld();
    if(!World || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    APlayerController* PC=Cast<APlayerController>(Pawn->GetController());
    if(!PC || !Pawn->FirstPersonCamera)return;
    const bool bBreakMode=FParse::Param(FCommandLine::Get(),TEXT("HouseGreatRoomPillowBreakReview"));
    const bool bHeavyMode=FParse::Param(FCommandLine::Get(),TEXT("HouseGreatRoomPillowHeavyReview"));
    static FRoomCushionReviewState S;
    const double Now=World->GetTimeSeconds();
    if(S.World.Get()!=World)
    {
        S=FRoomCushionReviewState();S.World=World;S.StepStarted=Now;
        S.Evidence=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),
            bBreakMode?TEXT("HouseGreatRoomPillowBreakReview"):
                bHeavyMode?TEXT("HouseGreatRoomPillowHeavyReview"):TEXT("HouseGreatRoomPillowReview")));
        IFileManager::Get().MakeDirectory(*S.Evidence,true);
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
        Pawn->SetActorEnableCollision(false);
    }
    if(S.Step<0)return;
    const float Age=Now-S.StepStarted;
    auto Next=[&](int32 Step){S.Step=Step;S.StepStarted=Now;};
    if(S.SofaIndex>=UE_ARRAY_COUNT(RoomSofas))
    {
        TSharedPtr<FJsonObject> Report=MakeShared<FJsonObject>();
        Report->SetArrayField(TEXT("runs"),S.Runs);
        FString Json;FJsonSerializer::Serialize(Report.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json,*FPaths::Combine(S.Evidence,TEXT("result.json")));
        S.Step=-1;FPlatformMisc::RequestExitWithStatus(false,0);return;
    }
    if(S.Step==0)
    {
        if(Age<2.f)return;
        S.Sofa.Reset();S.Pillows.Reset();S.PillowStarts.Reset();S.PeakSpeeds.Reset();S.PeakRises.Reset();
        for(TActorIterator<AHouseInteractiveProp> It(World);It;++It)
        {
            if(It->PropId==FName(RoomSofas[S.SofaIndex]))S.Sofa=*It;
            for(int32 P=0;P<2;++P)if(It->PropId==FName(RoomPillows[S.SofaIndex][P]))S.Pillows.Add(*It);
        }
        if(!S.Sofa.IsValid() || S.Pillows.Num()!=2)
        {
            UE_LOG(LogTemp,Error,TEXT("ROOM_CUSHION_REVIEW missing couch or pillows index=%d"),S.SofaIndex);
            ++S.SofaIndex;S.StepStarted=Now;return;
        }
        const FVector Center=S.Sofa->Body->Bounds.Origin;
        const float Front=S.SofaIndex==0?1.f:-1.f;
        const FVector View=Center+FVector(0,Front*360.f,20.f);
        Pawn->SetActorLocation(View-FVector(0,0,Pawn->BaseEyeHeight),false,nullptr,ETeleportType::TeleportPhysics);
        TierAim(Pawn,PC,Center+FVector(0,0,30.f));
        for(const TWeakObjectPtr<AHouseInteractiveProp>& Pillow:S.Pillows)
        {
            S.PillowStarts.Add(Pillow->Body->Bounds.Origin);S.PeakSpeeds.Add(0.f);S.PeakRises.Add(0.f);
        }
        Next(1);return;
    }
    AHouseInteractiveProp* Sofa=S.Sofa.Get();if(!Sofa){++S.SofaIndex;Next(0);return;}
    auto Capture=[&](const TCHAR* Label)
    {
        if(!FScreenshotRequest::IsScreenshotRequested())
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Evidence,
                FString::Printf(TEXT("couch_%d_%s.png"),S.SofaIndex,Label)),false,false);
    };
    if(S.Step==1)
    {
        if(Age<.8f)return;
        Capture(TEXT("before"));Next(2);return;
    }
    if(S.Step==2)
    {
        if(Age<.3f)return;
        FHitResult Hit(Sofa,Sofa->Body,Sofa->Body->Bounds.Origin,-FVector::ForwardVector);
        const float Damage=bBreakMode?Sofa->BreakHealth*1.1f:
            bHeavyMode?Sofa->BreakHealth*2.f/3.f+.1f:Sofa->BreakHealth/3.f+.1f;
        UGameplayStatics::ApplyPointDamage(Sofa,Damage,FVector::ForwardVector,
            Hit,PC,Pawn,UDamageType::StaticClass());
        Next(3);return;
    }
    if(S.Step==3)
    {
        for(int32 I=0;I<S.Pillows.Num();++I)
            if(AHouseInteractiveProp* Pillow=S.Pillows[I].Get())
            {
                S.PeakSpeeds[I]=FMath::Max(S.PeakSpeeds[I],float(Pillow->Body->GetPhysicsLinearVelocity().Size()));
                S.PeakRises[I]=FMath::Max(S.PeakRises[I],float(Pillow->Body->Bounds.Origin.Z-S.PillowStarts[I].Z));
            }
        if(Age>.12f && Age<.4f)Capture(TEXT("after_012"));
        if(Age>.8f && Age<1.f)Capture(TEXT("after_080"));
        if(Age<2.f)return;
        Capture(TEXT("settled"));
        TSharedPtr<FJsonObject> Row=MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("sofa"),Sofa->PropId.ToString());
        Row->SetNumberField(TEXT("sofa_damage_stage"),Sofa->DamageStage);
        Row->SetBoolField(TEXT("sofa_broken"),Sofa->bBroken);
        TArray<TSharedPtr<FJsonValue>> Pillows;
        for(int32 I=0;I<S.Pillows.Num();++I)
            if(AHouseInteractiveProp* Pillow=S.Pillows[I].Get())
            {
                TSharedPtr<FJsonObject> Entry=MakeShared<FJsonObject>();
                Entry->SetStringField(TEXT("id"),Pillow->PropId.ToString());
                Entry->SetNumberField(TEXT("mass_kg"),Pillow->MassKg);
                Entry->SetBoolField(TEXT("awake"),Pillow->bPhysicsActive);
                Entry->SetNumberField(TEXT("peak_speed_cm_s"),S.PeakSpeeds[I]);
                Entry->SetNumberField(TEXT("peak_rise_cm"),S.PeakRises[I]);
                Entry->SetNumberField(TEXT("end_displacement_cm"),FVector::Dist(Pillow->Body->Bounds.Origin,S.PillowStarts[I]));
                Pillows.Add(MakeShared<FJsonValueObject>(Entry));
            }
        Row->SetArrayField(TEXT("pillows"),Pillows);
        S.Runs.Add(MakeShared<FJsonValueObject>(Row));
        ++S.SofaIndex;Next(0);return;
    }
}


namespace
{
struct FFlagSurveyState
{
    TWeakObjectPtr<UWorld> World;
    TArray<TWeakObjectPtr<UStaticMeshComponent>> Found;
    TArray<FVector> Views; TArray<FVector> Targets;
    TArray<TSharedPtr<FJsonValue>> Rows;
    int32 Index=-1, Placed=-1; double StepStarted=0; FString Dir; bool bScanned=false;
};
bool FlagWord(const FString& Text)
{
    const FString L=Text.ToLower();
    return L.Contains(TEXT("flag")) || L.Contains(TEXT("banner")) || L.Contains(TEXT("pennant")) || L.Contains(TEXT("tapestry"))
        || L.Contains(TEXT("colorado")) || L.Contains(TEXT("tennessee")) || L.Contains(TEXT("usa")) || L.Contains(TEXT("texas"));
}
}

void TickHouseFlagSurvey(AStoryFirstPersonCharacter* Pawn)
{
    if(!Pawn->HasAuthority() || !Pawn->IsLocallyControlled())return;
    UWorld* World=Pawn->GetWorld();
    if(!World || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    APlayerController* PC=Cast<APlayerController>(Pawn->GetController());
    if(!PC || !Pawn->FirstPersonCamera)return;
    static FFlagSurveyState F;
    const double Now=World->GetTimeSeconds();
    if(F.World.Get()!=World){F=FFlagSurveyState();F.World=World;F.StepStarted=Now;
        F.Dir=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("HouseFlagSurvey")));IFileManager::Get().MakeDirectory(*F.Dir,true);}
    if(F.Index==-2)return;
    if(!F.bScanned)
    {
        if(Now-F.StepStarted<3.f)return;
        F.bScanned=true;
        for(TObjectIterator<UStaticMeshComponent> It;It;++It)
        {
            UStaticMeshComponent* C=*It;
            if(!IsValid(C) || C->GetWorld()!=World || !C->GetStaticMesh() || C->IsA<UInstancedStaticMeshComponent>())continue;
            FString Materials;bool bMatch=FlagWord(C->GetStaticMesh()->GetName())||FlagWord(GetNameSafe(C->GetOwner()));
            for(int32 Slot=0;Slot<C->GetNumMaterials();++Slot){const FString M=GetNameSafe(C->GetMaterial(Slot));Materials+=M+TEXT(";");bMatch|=FlagWord(M);}
            // Also any thin, wall-hung sheet on the ground floor (flags may be unnamed).
            const FVector Ext=C->Bounds.BoxExtent;
            const bool bSheet=C->Bounds.Origin.Z<330.f && Ext.GetMin()<4.f && Ext.GetMax()>25.f && Ext.GetMax()<160.f
                && FMath::Min(FMath::Max(Ext.X,Ext.Y),Ext.Z)>12.f;
            if(!bMatch && !bSheet)continue;
            TSharedPtr<FJsonObject> Row=MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("owner"),GetNameSafe(C->GetOwner()));
            Row->SetStringField(TEXT("owner_class"),C->GetOwner()?C->GetOwner()->GetClass()->GetName():TEXT(""));
            Row->SetStringField(TEXT("component"),C->GetName());
            Row->SetStringField(TEXT("mesh"),C->GetStaticMesh()->GetName());
            Row->SetStringField(TEXT("materials"),Materials);
            Row->SetStringField(TEXT("center"),C->Bounds.Origin.ToString());
            Row->SetStringField(TEXT("extent"),C->Bounds.BoxExtent.ToString());
            Row->SetNumberField(TEXT("index"),F.Found.Num());
            F.Rows.Add(MakeShared<FJsonValueObject>(Row));
            F.Found.Add(C);
            // View from the open side of the thinnest axis.
            const FVector E=C->Bounds.BoxExtent, O=C->Bounds.Origin;
            const FVector Axis=E.X<E.Y?FVector::ForwardVector:FVector::RightVector;
            const float Dist=FMath::Clamp(E.GetMax()*3.f,140.f,320.f);
            FVector Best=O+Axis*Dist; float BestFree=-1.f;
            for(const FVector& Dir:{Axis,-Axis})
            {
                FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(FlagSurvey),false,C->GetOwner());
                const float Free=World->LineTraceSingleByChannel(Hit,O+Dir*(E.GetMin()+3.f),O+Dir*400.f,ECC_Visibility,Q)?Hit.Distance:400.f;
                if(Free>BestFree){BestFree=Free;Best=O+Dir*FMath::Min(Dist,FMath::Max(60.f,Free-25.f));}
            }
            F.Views.Add(Best+FVector(0,0,-15));F.Targets.Add(O);
        }
        TSharedPtr<FJsonObject> Report=MakeShared<FJsonObject>();Report->SetArrayField(TEXT("flags"),F.Rows);
        FString Json;FJsonSerializer::Serialize(Report.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json,*FPaths::Combine(F.Dir,TEXT("flags.json")));
        F.Index=0;F.StepStarted=Now;
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);Pawn->SetActorEnableCollision(false);
        return;
    }
    if(F.Index>=F.Found.Num() || F.Index>=80){F.Index=-2;FPlatformMisc::RequestExitWithStatus(false,0);return;}
    const float Age=Now-F.StepStarted;
    if(F.Placed!=F.Index)
    {
        F.Placed=F.Index;F.StepStarted=Now;
        Pawn->SetActorLocation(F.Views[F.Index]-FVector(0,0,Pawn->BaseEyeHeight),false,nullptr,ETeleportType::TeleportPhysics);
        PC->SetControlRotation((F.Targets[F.Index]-F.Views[F.Index]).Rotation());
        return;
    }
    PC->SetControlRotation((F.Targets[F.Index]-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
    if(Age>1.2f && !FScreenshotRequest::IsScreenshotRequested())
    {
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(F.Dir,FString::Printf(TEXT("flag_%02d.png"),F.Index)),false,false);
        ++F.Index;F.StepStarted=Now;
    }
}


namespace
{
const TCHAR* const FlagTargets[]={TEXT("SMF_Upper_ALL_038"),TEXT("SMF_Upper_ALL_039"),TEXT("SMF_Entrance_ALL_092"),TEXT("SMF_Entrance_ALL_097"),TEXT("HallEnd_BudLightFlag")};
struct FFlagReviewState
{
    TWeakObjectPtr<UWorld> World; TWeakObjectPtr<AHouseInteractiveProp> Prop;
    TArray<TSharedPtr<FJsonValue>> Runs; TSharedPtr<FJsonObject> Row;
    int32 Run=0, Step=0; double StepStarted=0, Started=0; FString Dir; TSet<FString> Taken;
    FVector View=FVector::ZeroVector, Walk=FVector::ZeroVector; float PeakHit=0.f; bool bCanWalk=false;
};
}

void TickHouseFlagReview(AStoryFirstPersonCharacter* Pawn)
{
    if(!Pawn->HasAuthority() || !Pawn->IsLocallyControlled())return;
    UWorld* World=Pawn->GetWorld();
    if(!World || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    APlayerController* PC=Cast<APlayerController>(Pawn->GetController());
    if(!PC || !Pawn->FirstPersonCamera)return;
    static FFlagReviewState F;
    const double Now=World->GetTimeSeconds();
    if(F.World.Get()!=World){F=FFlagReviewState();F.World=World;F.StepStarted=F.Started=Now;
        F.Dir=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("HouseFlagReview")));IFileManager::Get().MakeDirectory(*F.Dir,true);}
    if(F.Step<0)return;
    const float Age=Now-F.StepStarted;
    auto Next=[&](int32 S){F.Step=S;F.StepStarted=Now;};
    auto Shot=[&](const FString& Name){ if(F.Taken.Contains(Name)||FScreenshotRequest::IsScreenshotRequested())return; F.Taken.Add(Name);
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(F.Dir,Name+TEXT(".png")),false,false); };
    if(F.Run>=UE_ARRAY_COUNT(FlagTargets) || Now-F.Started>260.)
    {
        if(FScreenshotRequest::IsScreenshotRequested() && Age<3.f)return;
        TSharedPtr<FJsonObject> Report=MakeShared<FJsonObject>();Report->SetArrayField(TEXT("flags"),F.Runs);
        FString Json;FJsonSerializer::Serialize(Report.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json,*FPaths::Combine(F.Dir,TEXT("result.json")));
        F.Step=-1;FPlatformMisc::RequestExitWithStatus(false,0);return;
    }
    const FString Target=FlagTargets[F.Run];
    const FString Tag=FString::Printf(TEXT("flag%d"),F.Run);
    AHouseInteractiveProp* Prop=F.Prop.Get();
    auto Look=[&](const FVector& At){PC->SetControlRotation((At-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());};
    if(F.Step==0)
    {
        if(Age<3.f && F.Run==0)return;
        Prop=nullptr;
        for(TActorIterator<AHouseInteractiveProp> It(World);It;++It)if(It->IntactMesh && It->IntactMesh->GetName().Contains(Target)){Prop=*It;break;}
        F.Row=MakeShared<FJsonObject>();F.Row->SetStringField(TEXT("target"),Target);
        if(!Prop){F.Row->SetStringField(TEXT("error"),TEXT("not found"));F.Runs.Add(MakeShared<FJsonValueObject>(F.Row));++F.Run;return;}
        F.Prop=Prop;
        F.Row->SetBoolField(TEXT("cloth"),Prop->IsClothFlag());
        F.Row->SetNumberField(TEXT("particles"),Prop->ClothFlag?Prop->ClothFlag->GetParticleCount():0);
        F.Row->SetBoolField(TEXT("wall_mounted"),Prop->bWallMounted);
        const FVector E=Prop->Body->Bounds.BoxExtent,O=Prop->Body->Bounds.Origin;
        const FVector Axis=E.X<E.Y?FVector::ForwardVector:FVector::RightVector;
        FVector Best=O+Axis*200.f;float BestFree=-1.f;FVector Open=Axis;
        for(const FVector& Side:{Axis,-Axis})
        {
            FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(FlagReview),false,Prop);
            const float Free=World->LineTraceSingleByChannel(Hit,O+Side*(E.GetMin()+3.f),O+Side*400.f,ECC_Visibility,Q)?Hit.Distance:400.f;
            if(Free>BestFree){BestFree=Free;Open=Side;Best=O+Side*FMath::Min(240.f,FMath::Max(80.f,Free-25.f));}
        }
        F.View=Best;
        // Floor spot in front of the flag for the walk-into test.
        FHitResult Floor;FCollisionQueryParams Q2(SCENE_QUERY_STAT(FlagReviewFloor),false,Prop);
        F.bCanWalk=World->LineTraceSingleByChannel(Floor,O+Open*110.f,O+Open*110.f-FVector(0,0,400),ECC_Visibility,Q2)
            && Prop->Body->Bounds.GetBox().Min.Z<Floor.ImpactPoint.Z+170.f;
        F.Walk=F.bCanWalk?Floor.ImpactPoint+FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f):FVector::ZeroVector;
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);Pawn->SetActorEnableCollision(false);
        Pawn->SetActorLocation(F.View-FVector(0,0,Pawn->BaseEyeHeight),false,nullptr,ETeleportType::TeleportPhysics);
        Next(1);return;
    }
    if(!Prop){++F.Run;Next(0);return;}
    const FVector Center=Prop->ClothFlag&&Prop->ClothFlag->IsActive()?Prop->ClothFlag->GetClothBounds().GetCenter():Prop->Body->Bounds.Origin;
    if(F.Step==1){ Look(Prop->Body->Bounds.Origin); if(Age>2.f)Shot(Tag+TEXT("_0_idle")); if(Age<2.3f)return;
        if(F.bCanWalk)
        {
            Pawn->SetActorEnableCollision(true);Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            Pawn->SetActorLocation(F.Walk,false,nullptr,ETeleportType::TeleportPhysics);
            Next(2);
        } else Next(3);
        return; }
    if(F.Step==2)
    {
        // Walk into the flag (the player passes into it; the cloth gets pushed).
        const FVector To=(Prop->Body->Bounds.Origin-Pawn->GetActorLocation()).GetSafeNormal2D();
        if(Age<1.4f)Pawn->AddMovementInput(To,1.f);
        PC->SetControlRotation(FRotator(-20.f,To.Rotation().Yaw+35.f,0));
        if(Age>.9f)Shot(Tag+TEXT("_1_walk_into"));
        if(Age<2.f)return;
        F.Row->SetNumberField(TEXT("walk_displacement_cm"),Prop->ClothFlag?Prop->ClothFlag->GetMaxDisplacement():0);
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);Pawn->SetActorEnableCollision(false);
        Pawn->SetActorLocation(F.View-FVector(0,0,Pawn->BaseEyeHeight),false,nullptr,ETeleportType::TeleportPhysics);
        Next(3);return;
    }
    Look(Prop->Body->Bounds.Origin);
    if(F.Step==3)
    {
        if(Age<1.5f)return;
        FHitResult HitInfo;HitInfo.ImpactPoint=HitInfo.Location=Prop->Body->Bounds.Origin+FVector(0,0,-Prop->Body->Bounds.BoxExtent.Z*.3f);
        HitInfo.Component=Prop->Body;HitInfo.HitObjectHandle=FActorInstanceHandle(Prop);
        UGameplayStatics::ApplyPointDamage(Prop,1.f,(HitInfo.ImpactPoint-Pawn->FirstPersonCamera->GetComponentLocation()).GetSafeNormal(),HitInfo,PC,Pawn,UDamageType::StaticClass());
        F.PeakHit=0.f;Next(4);return;
    }
    if(F.Step==4)
    {
        if(Prop->ClothFlag)F.PeakHit=FMath::Max(F.PeakHit,Prop->ClothFlag->GetMaxDisplacement());
        if(Age>.08f)Shot(Tag+TEXT("_2_hit_008"));
        if(Age>.3f)Shot(Tag+TEXT("_3_hit_030"));
        if(Age<1.6f)return;
        F.Row->SetNumberField(TEXT("hit_peak_displacement_cm"),F.PeakHit);
        F.Row->SetNumberField(TEXT("rest_displacement_after_hit_cm"),Prop->ClothFlag?Prop->ClothFlag->GetMaxDisplacement():0);
        F.Row->SetNumberField(TEXT("stage_after_hit"),Prop->DamageStage);
        Prop->WakeForInteraction(Pawn);
        F.Row->SetBoolField(TEXT("pulled_down"),Prop->ClothFlag&&Prop->ClothFlag->IsReleased());
        Next(5);return;
    }
    if(F.Step==5)
    {
        PC->SetControlRotation((Center-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation()+FRotator(-8,0,0));
        if(Age>.25f)Shot(Tag+TEXT("_4_pull_025"));
        if(Age>.6f)Shot(Tag+TEXT("_5_pull_060"));
        if(Age>3.f)Shot(Tag+TEXT("_6_on_floor"));
        if(Age<3.3f)return;
        if(Prop->ClothFlag&&Prop->ClothFlag->IsActive())
        {
            const FBox B=Prop->ClothFlag->GetClothBounds();
            F.Row->SetNumberField(TEXT("down_top_z"),B.Max.Z);F.Row->SetNumberField(TEXT("down_bottom_z"),B.Min.Z);
            F.Row->SetNumberField(TEXT("down_height_cm"),B.GetSize().Z);
        }
        F.Runs.Add(MakeShared<FJsonValueObject>(F.Row));
        ++F.Run;Next(0);return;
    }
}

void TickHouseCarryTiersReview(AStoryFirstPersonCharacter* Pawn)
{
    if(Pawn && FParse::Param(FCommandLine::Get(),TEXT("HouseFrontBushReview"))){ TickHouseFrontBushReview(Pawn); return; }
    if(Pawn && FParse::Param(FCommandLine::Get(),TEXT("HouseConnectionsReview"))){ TickHouseConnectionsReview(Pawn); return; }
    if(Pawn && FParse::Param(FCommandLine::Get(),TEXT("NoodleWingSmoke"))){ TickHouseNoodleWingReview(Pawn); return; }
    if(Pawn && (FParse::Param(FCommandLine::Get(),TEXT("HouseGreatRoomPillowReview"))
        || FParse::Param(FCommandLine::Get(),TEXT("HouseGreatRoomPillowBreakReview"))
        || FParse::Param(FCommandLine::Get(),TEXT("HouseGreatRoomPillowHeavyReview")))){ TickHouseGreatRoomPillowReview(Pawn); return; }
    if(Pawn && FParse::Param(FCommandLine::Get(),TEXT("HouseFurniturePushReview"))){ TickHouseFurniturePushReview(Pawn); return; }
    if(Pawn && (FParse::Param(FCommandLine::Get(),TEXT("HouseBreakReview"))
        || FParse::Param(FCommandLine::Get(),TEXT("HouseGreatRoomCushionReview")))){ TickHouseBreakReview(Pawn); return; }
    if(Pawn && FParse::Param(FCommandLine::Get(),TEXT("HouseBlackHallSurvey"))){ TickHouseBlackHallSurvey(Pawn); return; }
    if(Pawn && FParse::Param(FCommandLine::Get(),TEXT("HouseBlackHallReview"))){ TickHouseBlackHallReview(Pawn); return; }
    if(Pawn && FParse::Param(FCommandLine::Get(),TEXT("HouseFlagSurvey"))){ TickHouseFlagSurvey(Pawn); return; }
    if(Pawn && FParse::Param(FCommandLine::Get(),TEXT("HouseFlagReview"))){ TickHouseFlagReview(Pawn); return; }
    if(Pawn && FParse::Param(FCommandLine::Get(),TEXT("HouseDetailSurvey"))){ TickHouseDetailSurvey(Pawn); return; }
    if(Pawn && FParse::Param(FCommandLine::Get(),TEXT("GreatRoomCollisionReview"))){ TickHouseGreatRoomCollisionReview(Pawn); return; }
    if(Pawn && FParse::Param(FCommandLine::Get(),TEXT("FrontDoubleDoorReview"))){ TickHouseFrontDoubleDoorReview(Pawn); return; }
    if(Pawn && FParse::Param(FCommandLine::Get(),TEXT("HouseExteriorWalkReview"))){ TickHouseExteriorWalkReview(Pawn); return; }
    if(Pawn && FParse::Param(FCommandLine::Get(),TEXT("HouseGadgetSmoke"))){ TickHouseGadgetSmoke(Pawn); return; }
    if(Pawn && FParse::Param(FCommandLine::Get(),TEXT("HouseStairSurvey"))){ TickHouseStairSurvey(Pawn); return; }
    if(Pawn && FParse::Param(FCommandLine::Get(),TEXT("HouseExteriorViews"))){ TickHouseExteriorViews(Pawn); return; }
    if(Pawn && (FParse::Param(FCommandLine::Get(),TEXT("HouseTreesReview")) || FParse::Param(FCommandLine::Get(),TEXT("HouseTreesNetworkReview")))){ TickHouseTreesReview(Pawn); return; }
    if(!Pawn || !Pawn->HasAuthority() || !Pawn->IsLocallyControlled()
        || !FParse::Param(FCommandLine::Get(),TEXT("HouseCarryTiersReview")))return;
    UWorld* World=Pawn->GetWorld();
    if(!World || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    APlayerController* PC=Cast<APlayerController>(Pawn->GetController());
    UHouseFurnitureInteractionComponent* Mover=Pawn->FurnitureInteraction;
    if(!PC || !Mover || !Pawn->FirstPersonCamera || !Pawn->FirstPersonArms)return;
    static FTierReviewState S;
    const double Now=World->GetTimeSeconds();
    if(S.World.Get()!=World)
    {
        S=FTierReviewState();S.World=World;S.Started=S.StepStarted=Now;S.WallStarted=FPlatformTime::Seconds();
        S.Report=MakeShared<FJsonObject>();
        S.Evidence=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("HouseCarryTiersReview")));
        IFileManager::Get().MakeDirectory(*S.Evidence,true);
    }
    if(S.Step<0)return;
    auto Next=[&](int32 Step){S.Step=Step;S.StepStarted=Now;};
    auto FinishTier=[&]()
    {
        S.Tiers.Add(MakeShared<FJsonValueObject>(S.TierReport));
        if(AHouseInteractiveProp* Old=S.Prop.Get())Old->Destroy();
        ++S.Tier;Next(S.Tier>2?100:1);
    };
    const float Age=Now-S.StepStarted;
    if(S.Step!=S.LastStep){S.LastStep=S.Step;UE_LOG(LogTemp,Display,TEXT("CARRY_TIERS_REVIEW tier=%d step=%d t=%.2f"),S.Tier,S.Step,Now-S.Started);}
    if(S.Step==100 || FPlatformTime::Seconds()-S.WallStarted>150.)
    {
        if(FScreenshotRequest::IsScreenshotRequested() && Age<3.f)return;
        S.Report->SetArrayField(TEXT("tiers"),S.Tiers);
        S.Report->SetArrayField(TEXT("captures"),S.Captures);
        S.Report->SetBoolField(TEXT("completed"),S.Step==100);
        FString Json;FJsonSerializer::Serialize(S.Report.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json,*FPaths::Combine(S.Evidence,TEXT("result.json")));
        UE_LOG(LogTemp,Display,TEXT("CARRY_TIERS_REVIEW_DONE completed=%d"),S.Step==100?1:0);
        S.Step=-1;
        FPlatformMisc::RequestExitWithStatus(false,0);
        return;
    }
    AHouseInteractiveProp* Prop=S.Prop.Get();
    const FString T=TierName(S.Tier);
    const bool bLarge=S.Tier==2;

    if(S.Step==0)
    {
        if(Age<2.f)return;
        if(ASeniorLobbyPlayerState* Selection=Pawn->GetPlayerState<ASeniorLobbyPlayerState>())
        { Selection->CharacterIndex=0;Pawn->ApplySelectedCharacter(); }
        PC->bShowMouseCursor=false;
        FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if(AStaticMeshActor* Floor=World->SpawnActor<AStaticMeshActor>(TierOrigin-FVector(0,0,20),FRotator::ZeroRotator,Params))
        {
            Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
            Floor->SetActorScale3D(FVector(80,80,.4f));
            Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
            S.Transients.Add(Floor);
        }
        for(const FVector& Offset:{FVector(150,-180,350),FVector(220,220,450),FVector(-200,0,400)})
            if(APointLight* Lamp=World->SpawnActor<APointLight>(TierOrigin+Offset,FRotator::ZeroRotator,Params))
            {
                UPointLightComponent* Bulb=Cast<UPointLightComponent>(Lamp->GetLightComponent());
                Bulb->SetMobility(EComponentMobility::Movable);Bulb->SetIntensityUnits(ELightUnits::Lumens);
                Bulb->SetIntensity(160000.f);Bulb->SetAttenuationRadius(2500.f);Bulb->SetCastShadows(false);
                S.Transients.Add(Lamp);
            }
        FPostProcessSettings& Exposure=Pawn->FirstPersonCamera->PostProcessSettings;
        Exposure.bOverride_AutoExposureMethod=true;Exposure.AutoExposureMethod=AEM_Basic;
        Exposure.bOverride_AutoExposureMinBrightness=true;Exposure.AutoExposureMinBrightness=8.f;
        Exposure.bOverride_AutoExposureMaxBrightness=true;Exposure.AutoExposureMaxBrightness=8.f;
        Pawn->FirstPersonCamera->PostProcessBlendWeight=1.f;
        Next(1);return;
    }
    if(S.Step==1)
    {
        // Tier setup: fresh prop, pawn at the start mark facing +X.
        S.TierReport=MakeShared<FJsonObject>();
        S.TierReport->SetStringField(TEXT("tier"),T);
        Mover->ReleaseGrab();
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Pawn->SetActorLocation(TierOrigin+FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f),
            false,nullptr,ETeleportType::TeleportPhysics);
        PC->SetControlRotation(FRotator::ZeroRotator);
        S.Prop=SpawnTierProp(S,World,S.Tier);
        S.PeakSpeed=S.MaxLift=S.MaxTilt=S.WorstHand=S.WorstTarget=S.WorstCamToGrip=0.f;
        if(!S.Prop.IsValid()){S.TierReport->SetStringField(TEXT("error"),TEXT("spawn failed"));FinishTier();return;}
        Next(2);return;
    }
    if(!Prop){S.TierReport->SetStringField(TEXT("error"),TEXT("prop lost"));FinishTier();return;}
    const FBox Box=Prop->Body->Bounds.GetBox();
    // Aim at the near face: low for big pieces, centre for the rest.
    const FVector AimPoint=bLarge?FVector(Box.Min.X+2.f,Box.GetCenter().Y,Box.Min.Z+Box.GetSize().Z*.55f):Box.GetCenter();
    if(S.Step==2)
    {
        if(Age<.8f)return;
        TierAim(Pawn,PC,AimPoint);
        S.InitialAim=PC->GetControlRotation();
        const bool bFocused=Mover->FocusedFurnitureForInteraction()==Prop;
        S.TierReport->SetBoolField(TEXT("focused"),bFocused);
        if(!bFocused){S.TierReport->SetStringField(TEXT("error"),TEXT("not focusable"));TierCapture(S,T+TEXT("_focus_fail"));FinishTier();return;}
        TierCapture(S,T+TEXT("_0_before"));
        S.PropStartRotation=Prop->GetActorRotation();
        Mover->ToggleGrab();
        S.TierReport->SetBoolField(TEXT("held"),Mover->bHoldingFurniture);
        S.TierReport->SetNumberField(TEXT("carry_size"),int32(Mover->GetCarrySize()));
        S.TierReport->SetBoolField(TEXT("two_hands"),Mover->UsesTwoHands());
        if(!Mover->bHoldingFurniture){S.TierReport->SetStringField(TEXT("error"),TEXT("grab failed"));FinishTier();return;}
        FVector C,N;Mover->GetHandContact(true,C,N);S.GripStart=C;
        Next(3);return;
    }
    if(S.Step==3)
    {
        // Pick up / brace and heave.
        const float LiftEnd=S.Tier==0?.9f:(S.Tier==1?2.0f:2.4f);
        if(!bLarge)PC->SetControlRotation(FMath::Lerp(S.InitialAim,FRotator(-8,0,0),FMath::SmoothStep(.3f,1.3f,Age)));
        FVector C,N;Mover->GetHandContact(true,C,N);
        S.MaxLift=FMath::Max(S.MaxLift,float(C.Z-S.GripStart.Z));
        S.MaxTilt=FMath::Max(S.MaxTilt,float(FMath::RadiansToDegrees((Prop->GetActorQuat()*S.PropStartRotation.Quaternion().Inverse()).GetAngle())));
        if(Age>.18f)TierCapture(S,T+TEXT("_1_reach"));
        if(bLarge && Age>.6f)TierCapture(S,T+TEXT("_1b_brace"));
        if(bLarge && Age<.5f)S.TierReport->SetNumberField(TEXT("movement_scale_while_bracing"),Mover->GetMovementScale());
        if(Age<LiftEnd)return;
        TierCapture(S,T+TEXT("_2_hold"));
        S.TierReport->SetNumberField(TEXT("hold_hand_error_cm"),HandError(Pawn,Mover));
        S.TierReport->SetNumberField(TEXT("hold_carry_pose"),Mover->GetCarryPoseBlend());
        S.TierReport->SetNumberField(TEXT("hold_finger_grip"),Mover->GetFingerGripBlend());
        S.TierReport->SetNumberField(TEXT("movement_scale"),Mover->GetMovementScale());
        S.TierReport->SetNumberField(TEXT("phase_after_lift"),int32(Mover->GetCarryPhase()));
        S.TierReport->SetBoolField(TEXT("blocks_sprint"),Mover->BlocksSprint());
        S.WalkStart=Pawn->GetActorLocation();
        Next(4);return;
    }
    if(S.Step==4)
    {
        // Walk forward while carrying / dragging.
        // Large pieces are pulled (walk backward); the rest are carried forward.
        Pawn->AddMovementInput(bLarge?-FVector::ForwardVector:FVector::ForwardVector,1.f);
        { FVector C,N;Mover->GetHandContact(true,C,N);S.MaxLift=FMath::Max(S.MaxLift,float(C.Z-S.GripStart.Z)); }
        S.MaxTilt=FMath::Max(S.MaxTilt,float(FMath::RadiansToDegrees((Prop->GetActorQuat()*S.PropStartRotation.Quaternion().Inverse()).GetAngle())));
        { const float E=HandError(Pawn,Mover); if(E>S.WorstHand){S.WorstHand=E;S.TierReport->SetNumberField(TEXT("walk_worst_hand_at_s"),Age);
          S.TierReport->SetNumberField(TEXT("walk_worst_hand_speed"),Pawn->GetVelocity().Size2D());} }
        S.WorstTarget=FMath::Max(S.WorstTarget,Mover->GetDebugTargetError());
        { FVector C,N;Mover->GetHandContact(true,C,N);
          const FVector Local=Pawn->FirstPersonCamera->GetComponentTransform().InverseTransformPosition(C);
          const FVector Palm=Pawn->FirstPersonCamera->GetComponentTransform().InverseTransformPosition(Pawn->GetFurnitureHandGripPoint(true));
          if(Age>.9f && Age<.95f){S.TierReport->SetStringField(TEXT("walk_contact_in_camera"),Local.ToString());S.TierReport->SetStringField(TEXT("walk_palm_in_camera"),Palm.ToString());}
          S.WorstCamToGrip=FMath::Max(S.WorstCamToGrip,float(Local.Size())); }
        if(Age>.9f)TierCapture(S,T+TEXT("_3_walk"));
        if(Age<1.6f)return;
        S.TierReport->SetNumberField(TEXT("walk_speed_cm_s"),FVector::Dist2D(Pawn->GetActorLocation(),S.WalkStart)/Age);
        S.TierReport->SetNumberField(TEXT("walk_worst_hand_error_cm"),S.WorstHand);
        S.TierReport->SetNumberField(TEXT("walk_end_hand_error_cm"),HandError(Pawn,Mover));
        S.TierReport->SetNumberField(TEXT("walk_worst_target_error_cm"),S.WorstTarget);
        S.TierReport->SetNumberField(TEXT("walk_worst_contact_distance_from_camera_cm"),S.WorstCamToGrip);
        S.TierReport->SetBoolField(TEXT("still_held_after_walk"),Mover->bHoldingFurniture);
        S.TierReport->SetNumberField(TEXT("prop_bottom_above_floor_cm"),Prop->Body->Bounds.GetBox().Min.Z-TierFloorZ);
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Next(bLarge?7:5);return;
    }
    if(S.Step==5)
    {
        // Set down (E), animated.
        if(Age<.02f){Mover->ToggleGrab();S.TierReport->SetNumberField(TEXT("setdown_phase"),int32(Mover->GetCarryPhase()));return;}
        if(Age>(S.Tier==0?.18f:.33f))TierCapture(S,T+TEXT("_4_setdown"));
        if(Age<1.6f)return;
        S.TierReport->SetBoolField(TEXT("released_after_setdown"),!Mover->bHoldingFurniture);
        S.TierReport->SetNumberField(TEXT("setdown_bottom_above_floor_cm"),Prop->Body->Bounds.GetBox().Min.Z-TierFloorZ);
        S.TierReport->SetNumberField(TEXT("setdown_speed_cm_s"),Prop->Body->GetPhysicsLinearVelocity().Size());
        Next(6);return;
    }
    if(S.Step==6)
    {
        // Pick it up again for the throw.
        if(Age<.3f)return;
        if(Age<.35f)
        {
            TierAim(Pawn,PC,Prop->Body->Bounds.Origin);
            if(Mover->FocusedFurnitureForInteraction()==Prop)Mover->ToggleGrab();
            S.InitialAim=PC->GetControlRotation();
            return;
        }
        PC->SetControlRotation(FMath::Lerp(S.InitialAim,FRotator(-2,0,0),FMath::SmoothStep(.3f,1.2f,Age)));
        if(Age<(S.Tier==0?1.f:2.f))return;
        S.TierReport->SetBoolField(TEXT("regrabbed"),Mover->bHoldingFurniture);
        if(!Mover->bHoldingFurniture){FinishTier();return;}
        Next(7);return;
    }
    if(S.Step==7)
    {
        // Throw / shove (LMB), with wind-up and follow-through captures.
        const float Wind=S.Tier==0?.2f:(S.Tier==1?.36f:.28f);
        if(Age<.02f)
        {
            if(bLarge)PC->SetControlRotation(FRotator(-5,0,0));
            S.ThrowStart=Prop->GetActorLocation();
            Mover->ThrowHeldFurniture();
            S.TierReport->SetNumberField(TEXT("throw_phase"),int32(Mover->GetCarryPhase()));
            return;
        }
        if(Age>Wind*.7f)TierCapture(S,T+TEXT("_5_windup"));
        if(Age>Wind+.1f)TierCapture(S,T+TEXT("_6_follow"));
        S.PeakSpeed=FMath::Max(S.PeakSpeed,float(Prop->Body->GetPhysicsLinearVelocity().Size()));
        if(Age<3.f)return;
        S.TierReport->SetBoolField(TEXT("released_after_throw"),!Mover->bHoldingFurniture);
        S.TierReport->SetNumberField(TEXT("throw_peak_speed_cm_s"),S.PeakSpeed);
        S.TierReport->SetNumberField(TEXT("throw_horizontal_distance_cm"),FVector::Dist2D(Prop->GetActorLocation(),S.ThrowStart));
        S.TierReport->SetNumberField(TEXT("max_grip_rise_cm"),S.MaxLift);
        S.TierReport->SetNumberField(TEXT("max_tilt_deg"),S.MaxTilt);
        TierCapture(S,T+TEXT("_7_landed"));
        FinishTier();return;
    }
}
#else
void TickHouseCarryTiersReview(AStoryFirstPersonCharacter*) {}
void TickHouseFurniturePushReview(AStoryFirstPersonCharacter*) {}
void TickHouseBreakReview(AStoryFirstPersonCharacter*) {}
void TickHouseFlagSurvey(AStoryFirstPersonCharacter*) {}
void TickHouseFlagReview(AStoryFirstPersonCharacter*) {}
#endif
