// Opt-in native house-prop checks. The test uses an isolated user directory and
// transient fixtures; it never saves or edits the authored map.
#include "HouseInteractiveProp.h"
#include "HouseDamageSubsystem.h"
#include "HouseFurnitureInteractionComponent.h"
#include "HouseFixtureSmoke.h"
#include "SeniorDouli.h"
#include "SeniorSauce.h"
#include "SeniorLobby.h"
#include "StoryCampaign.h"

#if !UE_BUILD_SHIPPING
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/DamageEvents.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
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
struct FHouseInteractivitySmoke
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AHouseInteractiveProp> Target;
    TWeakObjectPtr<AHouseInteractiveProp> Thrown;
    TWeakObjectPtr<AHouseInteractiveProp> Representative;
    TWeakObjectPtr<AActor> SavedObservationViewTarget;
    TArray<TWeakObjectPtr<AActor>> Transients;
    int32 Stage = 0, LastStage = INDEX_NONE, CountBefore = 0;
    int32 InstalledProps = 0, InstalledMovables = 0, InstalledBreakables = 0;
    int32 InstalledBrokenVariants = 0, InitiallySimulating = 0;
    int32 SavedCharacter = 0, SavedLoadout = 0;
    float SavedGravity = 0, SavedWalkSpeed = 0, ThrowSpeed = 0;
    double ThrowHoldReadySince = -1., ThrowReleasedAt = 0., LastFlightSampleAt = -1.;
    float ThrowClosestTargetCm = BIG_NUMBER;
    TSharedPtr<FJsonObject> ThrowStartReport;
    TArray<TSharedPtr<FJsonValue>> ThrowFlightReports;
    bool bSavedLookIgnored = false, bSavedMoveIgnored = false, bSavedCursor = false;
    bool bSauceHit = false, bDouliHit = false, bHeldRelease = false;
    bool bThrownImpact = false, bCountOnce = false, bIdleStable = false, bAuthoredVariant = false;
    bool bWallDetached = false, bSupportedPropFell = false, bSturdyPieceResisted = false, bSturdyMountResisted = false;
    float WallFallDistance = 0;
    FPostProcessSettings SavedCameraPostProcess;
    float SavedCameraPostProcessWeight = 0.f;
    bool bTestExposureActive = false;
    float SupportedCupFall = 0.f, SupportSeparation = 0.f;
    bool bDamageStages = false, bAuthoredDamageStages = false, bBrokenCapsulePass = false;
    bool bAuthoredHeldStageReleased = false, bAuthoredStagesSettled = false, bInHouseStagesSettled = false;
    float GroundFloorZ = 8000.f, GroundPeakSpeed = 0.f, GroundPeakAngular = 0.f;
    double GroundRestSince = -1.;
    TArray<TSharedPtr<FJsonValue>> GroundingReports;
    TArray<TSharedPtr<FJsonValue>> StageMaterialReports;
    TArray<TSharedPtr<FJsonValue>> InitialActivePropReports;
    double Started = 0, WallStarted = 0, StageStarted = 0;
    FVector IdlePosition = FVector::ZeroVector;
    FString Evidence, RepresentativeName;
    TArray<FString> Captures;
    TSharedPtr<FJsonObject> FixtureReport;
};

void PropSmokeRecordThrowFlight(FHouseInteractivitySmoke& S, double Now, const TCHAR* Phase)
{
    AHouseInteractiveProp* Thrown = S.Thrown.Get();
    AHouseInteractiveProp* Target = S.Target.Get();
    if (!Thrown || !Target) return;
    const FVector Center = Thrown->Body->Bounds.Origin;
    const FVector Velocity = Thrown->Body->GetPhysicsLinearVelocity();
    S.ThrowSpeed = FMath::Max(S.ThrowSpeed, float(Velocity.Size()));
    FVector ClosestPoint;
    const float Distance = Target->Body->GetClosestPointOnCollision(Center, ClosestPoint);
    if (Distance >= 0.f) S.ThrowClosestTargetCm = FMath::Min(S.ThrowClosestTargetCm, Distance);
    if (S.LastFlightSampleAt >= 0. && Now - S.LastFlightSampleAt < .04 && !Target->bBroken) return;
    S.LastFlightSampleAt = Now;
    TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("phase"), Phase);
    Row->SetNumberField(TEXT("secondsAfterRelease"), Now - S.ThrowReleasedAt);
    Row->SetStringField(TEXT("positionCm"), Center.ToString());
    Row->SetStringField(TEXT("velocityCmPerSecond"), Velocity.ToString());
    Row->SetStringField(TEXT("rotation"), Thrown->GetActorRotation().ToString());
    Row->SetNumberField(TEXT("centerDistanceToTargetSurfaceCm"), Distance);
    Row->SetBoolField(TEXT("targetBroken"), Target->bBroken);
    Row->SetNumberField(TEXT("targetHealth"), Target->Health);
    Row->SetNumberField(TEXT("targetDamageStage"), Target->DamageStage);
    Row->SetBoolField(TEXT("thrownBroken"), Thrown->bBroken);
    Row->SetBoolField(TEXT("gravityEnabled"), Thrown->Body->IsGravityEnabled());
    Row->SetBoolField(TEXT("physicsSimulating"), Thrown->Body->IsSimulatingPhysics());
    S.ThrowFlightReports.Add(MakeShared<FJsonValueObject>(Row));
}

void PropSmokeCapture(FHouseInteractivitySmoke& S, const TCHAR* Name)
{
    if (FScreenshotRequest::IsScreenshotRequested()) return;
    const FString Path = FPaths::Combine(S.Evidence, FString(Name) + TEXT(".png"));
    FScreenshotRequest::RequestScreenshot(Path, false, false);
    S.Captures.Add(Path);
}

void PropSmokeAim(AStoryFirstPersonCharacter* Pawn, APlayerController* PC, const FVector& Point)
{
    PC->SetControlRotation((Point - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
    if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
}

AHouseInteractiveProp* PropSmokeSpawn(FHouseInteractivitySmoke& S, UWorld* World,
    const FVector& Location, const FVector& Scale, const FString& Name, bool bMovable = true)
{
    const FTransform Transform(FRotator::ZeroRotator, Location, Scale);
    AHouseInteractiveProp* Prop = World->SpawnActorDeferred<AHouseInteractiveProp>(
        AHouseInteractiveProp::StaticClass(), Transform, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Prop) return nullptr;
    Prop->IntactMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    Prop->BrokenMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
    Prop->PropName = Name;
    Prop->PropId = FName(*Name);
    Prop->bMovable = bMovable;
    Prop->bBrokenVersionMovable = bMovable;
    Prop->MassKg = 12.f;
    Prop->FinishSpawning(Transform);
    S.Transients.Add(Prop);
    return Prop;
}

void PropSmokeRemove(TWeakObjectPtr<AHouseInteractiveProp>& Prop)
{
    if (Prop.IsValid()) Prop->Destroy();
    Prop.Reset();
}

void PropSmokePointHit(AHouseInteractiveProp* Prop, APlayerController* PC, AActor* Causer, float Amount = 8.f)
{
    FHitResult Hit(Prop, Prop->Body, Prop->Body->Bounds.Origin, -FVector::ForwardVector);
    UGameplayStatics::ApplyPointDamage(Prop, Amount, FVector::ForwardVector,
        Hit, PC, Causer, UDamageType::StaticClass());
}

bool PropSmokeAuditStageMaterials(FHouseInteractivitySmoke& S, AHouseInteractiveProp* Prop,
    const TCHAR* Location, FString& Failure)
{
    const UStaticMesh* Mesh = Prop->Body->GetStaticMesh();
    if (!Mesh) { Failure = TEXT("Material audit found a missing damage mesh"); return false; }
    TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("location"), Location);
    Row->SetStringField(TEXT("mesh"), Mesh->GetPathName());
    Row->SetNumberField(TEXT("damageStage"), Prop->DamageStage);
    TArray<TSharedPtr<FJsonValue>> Slots;
    const TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
    for (int32 Index = 0; Index < Materials.Num(); ++Index)
    {
        const FStaticMaterial& Slot = Materials[Index];
        const FName Name = Slot.ImportedMaterialSlotName.IsNone() ? Slot.MaterialSlotName : Slot.ImportedMaterialSlotName;
        UMaterialInterface* Actual = Prop->Body->GetMaterial(Index);
        const bool bCore = Name.ToString().StartsWith(TEXT("M_HI_"));
        // New core materials belong to the damage mesh and must survive all
        // intact per-instance overrides, including compacted slot positions.
        const bool bCorrect = !bCore || (Actual && Actual == Slot.MaterialInterface && Actual->GetFName() == Name);
        TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("slotName"), Name.ToString());
        Entry->SetStringField(TEXT("meshMaterial"), GetPathNameSafe(Slot.MaterialInterface.Get()));
        Entry->SetStringField(TEXT("componentMaterial"), GetPathNameSafe(Actual));
        Entry->SetBoolField(TEXT("exposedCore"), bCore);
        Entry->SetBoolField(TEXT("coreMaterialPreserved"), bCorrect);
        Slots.Add(MakeShared<FJsonValueObject>(Entry));
        if (!bCorrect)
            Failure = FString::Printf(TEXT("Damage core material changed at %s slot %s: mesh=%s component=%s"),
                *Mesh->GetName(), *Name.ToString(), *GetPathNameSafe(Slot.MaterialInterface.Get()), *GetPathNameSafe(Actual));
    }
    Row->SetArrayField(TEXT("slots"), Slots);
    S.StageMaterialReports.Add(MakeShared<FJsonValueObject>(Row));
    return Failure.IsEmpty();
}

// A rotated mesh's enclosing box contains empty corners. Measure the actual
// rendered positions, never those corners, when checking floor contact.
float PropSmokeVisibleBottom(UStaticMeshComponent* Body,FString* OutMethod=nullptr)
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
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HousePropSmokeVisibleSurface),true);
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

// Observe real simulation over several frames. The old variant screenshot test
// parked the authored chair, which could not catch suspended bodies or bad hulls.
int32 PropSmokeGrounding(FHouseInteractivitySmoke& S, AHouseInteractiveProp* Prop,
    double Now, float Age, const TCHAR* Location, FString& Failure)
{
    if (!Prop || !Prop->Body->IsSimulatingPhysics() || !Prop->Body->IsGravityEnabled())
    { Failure = TEXT("Authored damaged furnishing did not retain gravity and a live rigid body"); return -1; }
    const float Speed = Prop->Body->GetPhysicsLinearVelocity().Size();
    const float Angular = Prop->Body->GetPhysicsAngularVelocityInDegrees().Size();
    FString GapMethod;
    const float Gap = PropSmokeVisibleBottom(Prop->Body,&GapMethod) - S.GroundFloorZ;
    S.GroundPeakSpeed = FMath::Max(S.GroundPeakSpeed, Speed);
    S.GroundPeakAngular = FMath::Max(S.GroundPeakAngular, Angular);
    if (!FMath::IsFinite(Speed) || !FMath::IsFinite(Angular) || !FMath::IsFinite(Gap)
        || Speed > 1150.f || Angular > 300.f)
    { Failure = TEXT("Authored stage transition produced an extreme or invalid physics velocity"); return -1; }
    const bool bResting = Speed < 12.f && Angular < 18.f && Gap < 12.f && Gap > -30.f;
    if (!bResting) S.GroundRestSince = -1.;
    else if (S.GroundRestSince < 0.) S.GroundRestSince = Now;
    if (S.GroundRestSince >= 0. && Now - S.GroundRestSince > .45)
    {
        if (!PropSmokeAuditStageMaterials(S, Prop, Location, Failure)) return -1;
        TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("location"), Location);
        Row->SetStringField(TEXT("prop"), Prop->PropName);
        Row->SetNumberField(TEXT("damageStage"), Prop->DamageStage);
        Row->SetNumberField(TEXT("visibleMeshFloorGapCm"), Gap);
        Row->SetNumberField(TEXT("boundingBoxFloorGapCm"), Prop->Body->Bounds.GetBox().Min.Z-S.GroundFloorZ);
        Row->SetStringField(TEXT("floorGapMethod"), GapMethod);
        Row->SetStringField(TEXT("restingRotation"), Prop->GetActorRotation().ToString());
        Row->SetStringField(TEXT("restingPositionCm"), Prop->GetActorLocation().ToString());
        Row->SetNumberField(TEXT("measuredFloorZCm"), S.GroundFloorZ);
        Row->SetNumberField(TEXT("settledSpeedCmPerSecond"), Speed);
        Row->SetNumberField(TEXT("peakSpeedCmPerSecond"), S.GroundPeakSpeed);
        Row->SetNumberField(TEXT("peakAngularDegreesPerSecond"), S.GroundPeakAngular);
        Row->SetNumberField(TEXT("secondsToSettle"), Age);
        Row->SetBoolField(TEXT("gravityEnabled"), Prop->Body->IsGravityEnabled());
        S.GroundingReports.Add(MakeShared<FJsonValueObject>(Row));
        UE_LOG(LogTemp, Display, TEXT("HOUSE_PROP_GROUNDING location=%s id=%s stage=%d gap=%.2f speed=%.2f peak=%.2f angular=%.2f"),
            Location, *Prop->PropId.ToString(), Prop->DamageStage, Gap, Speed, S.GroundPeakSpeed, S.GroundPeakAngular);
        return 1;
    }
    if (Age > 5.f)
    {
        Failure = FString::Printf(TEXT("Authored stage %d failed to land and settle at %s: floor gap %.2f cm, speed %.2f, angular %.2f, position %s, rotation %s, floor Z %.2f"),
            Prop->DamageStage, Location, Gap, Speed, Angular, *Prop->GetActorLocation().ToString(),
            *Prop->GetActorRotation().ToString(), S.GroundFloorZ);
        return -1;
    }
    return 0;
}
}

static void RunHouseInteractivitySmoke(UWorld* World)
{
    if (!World || !FParse::Param(FCommandLine::Get(), TEXT("HouseInteractivitySmoke"))
        || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    static FHouseInteractivitySmoke S;
    if (S.World.Get() != World)
    {
        S = FHouseInteractivitySmoke(); S.World = World;
        S.Started = World->GetTimeSeconds(); S.WallStarted = FPlatformTime::Seconds();
        FParse::Value(FCommandLine::Get(), TEXT("HouseInteractivityEvidence="), S.Evidence, false);
        if (S.Evidence.IsEmpty()) S.Evidence = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HouseInteractivitySmoke"));
        S.Evidence = FPaths::ConvertRelativePathToFull(S.Evidence);
        IFileManager::Get().MakeDirectory(*S.Evidence, true);
    }
    if (S.Stage < 0) return;
    APlayerController* PC = World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn = PC ? Cast<AStoryFirstPersonCharacter>(PC->GetPawn()) : nullptr;
    ASeniorLobbyPlayerState* Selection = Pawn ? Pawn->GetPlayerState<ASeniorLobbyPlayerState>() : nullptr;
    UHouseDamageSubsystem* Damage = World->GetSubsystem<UHouseDamageSubsystem>();
    const double Now = World->GetTimeSeconds();
    auto Finish = [&](bool bPass, const FString& Detail)
    {
        if (PC && S.SavedObservationViewTarget.IsValid())
            PC->SetViewTarget(S.SavedObservationViewTarget.Get());
        if (Pawn && Pawn->FurnitureInteraction) Pawn->FurnitureInteraction->ReleaseGrab();
        if (Pawn && Pawn->FirstPersonCamera && S.bTestExposureActive)
        {
            Pawn->FirstPersonCamera->PostProcessSettings = S.SavedCameraPostProcess;
            Pawn->FirstPersonCamera->PostProcessBlendWeight = S.SavedCameraPostProcessWeight;
            S.bTestExposureActive = false;
        }
        if (Selection) { Selection->CharacterIndex = S.SavedCharacter; Selection->LoadoutIndex = S.SavedLoadout; }
        TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
        Report->SetBoolField(TEXT("passed"), bPass);
        Report->SetStringField(TEXT("detail"), Detail);
        Report->SetNumberField(TEXT("stage"), S.Stage);
        Report->SetNumberField(TEXT("seconds"), Now - S.Started);
        Report->SetStringField(TEXT("method"), TEXT("Native sauce and douli flights from the real character weapon input; existing physics-handle grab and throw; production damage callback while held; installed asset audit and representative variant. Transient setup outside the house, isolated UserDir, no map saves."));
        Report->SetNumberField(TEXT("installedProps"), S.InstalledProps);
        Report->SetNumberField(TEXT("installedMovables"), S.InstalledMovables);
        Report->SetNumberField(TEXT("installedBreakables"), S.InstalledBreakables);
        Report->SetNumberField(TEXT("installedBrokenVariants"), S.InstalledBrokenVariants);
        Report->SetNumberField(TEXT("initiallySimulatingProps"), S.InitiallySimulating);
        Report->SetArrayField(TEXT("initiallyActivePropStates"), S.InitialActivePropReports);
        Report->SetBoolField(TEXT("idlePropsStable"), S.bIdleStable);
        Report->SetBoolField(TEXT("nativeSauceBrokeProp"), S.bSauceHit);
        Report->SetBoolField(TEXT("nativeDouliBrokeProp"), S.bDouliHit);
        Report->SetBoolField(TEXT("repeatDamageCountedOnce"), S.bCountOnce);
        Report->SetBoolField(TEXT("heldBreakReleasedHandle"), S.bHeldRelease);
        Report->SetBoolField(TEXT("nativeThrownPropBrokeTarget"), S.bThrownImpact);
        Report->SetNumberField(TEXT("nativeThrowSpeedCmPerSecond"), S.ThrowSpeed);
        Report->SetNumberField(TEXT("thrownClosestCenterToTargetSurfaceCm"), S.ThrowClosestTargetCm < BIG_NUMBER ? S.ThrowClosestTargetCm : -1.f);
        if (S.ThrowStartReport.IsValid()) Report->SetObjectField(TEXT("throwStart"), S.ThrowStartReport);
        Report->SetArrayField(TEXT("throwFlight"), S.ThrowFlightReports);
        Report->SetBoolField(TEXT("authoredBrokenVariantVerified"), S.bAuthoredVariant);
        Report->SetBoolField(TEXT("wallPictureDetachedIntactThenBrokeOnce"), S.bWallDetached);
        Report->SetNumberField(TEXT("detachedPictureFellCm"), S.WallFallDistance);
        Report->SetBoolField(TEXT("supportedContentsWokeAndFell"), S.bSupportedPropFell);
        Report->SetNumberField(TEXT("supportedCupFallCm"), S.SupportedCupFall);
        Report->SetNumberField(TEXT("supportSeparationCm"), S.SupportSeparation);
        Report->SetBoolField(TEXT("sturdyPieceRequiredRepeatedHits"), S.bSturdyPieceResisted);
        Report->SetBoolField(TEXT("sturdyWallMountRequiredRepeatedHits"), S.bSturdyMountResisted);
        Report->SetBoolField(TEXT("threeDamageStagesAndHeldStageRelease"), S.bDamageStages);
        Report->SetBoolField(TEXT("authoredDamageStagesVerified"), S.bAuthoredDamageStages);
        Report->SetBoolField(TEXT("authoredHeldStageReleased"), S.bAuthoredHeldStageReleased);
        Report->SetBoolField(TEXT("authoredDamageStagesPhysicallySettled"), S.bAuthoredStagesSettled);
        Report->SetBoolField(TEXT("inHouseDamageStagesPhysicallySettled"), S.bInHouseStagesSettled);
        Report->SetArrayField(TEXT("authoredStageGrounding"), S.GroundingReports);
        Report->SetArrayField(TEXT("authoredStageMaterials"), S.StageMaterialReports);
        Report->SetBoolField(TEXT("brokenFurnitureAllowsPlayerCapsule"), S.bBrokenCapsulePass);
        Report->SetStringField(TEXT("representativeAuthoredProp"), S.RepresentativeName);
        Report->SetNumberField(TEXT("totalBrokenCount"), Damage ? Damage->GetTotalBrokenCount() : -1);
        if (S.FixtureReport.IsValid()) Report->SetObjectField(TEXT("fixtures"), S.FixtureReport);
        TArray<TSharedPtr<FJsonValue>> Captures;
        for (const FString& Path : S.Captures) Captures.Add(MakeShared<FJsonValueString>(Path));
        Report->SetArrayField(TEXT("captures"), Captures);
        FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json, *FPaths::Combine(S.Evidence, TEXT("result.json")));
        UE_LOG(LogTemp, Display, TEXT("HOUSE_INTERACTIVITY_SMOKE_RESULT passed=%d stage=%d installed=%d sauce=%d douli=%d heldRelease=%d thrownImpact=%d detail=%s"),
            bPass ? 1 : 0, S.Stage, S.InstalledProps, S.bSauceHit ? 1 : 0, S.bDouliHit ? 1 : 0,
            S.bHeldRelease ? 1 : 0, S.bThrownImpact ? 1 : 0, *Detail);
        for (TWeakObjectPtr<AActor> Actor : S.Transients) if (Actor.IsValid()) Actor->Destroy();
        S.Stage = -1;
        FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
    };
    auto Stage = [&](int32 Next)
    {
        S.Stage = Next; S.StageStarted = Now;
        S.GroundRestSince = -1.; S.GroundPeakSpeed = 0.f; S.GroundPeakAngular = 0.f;
    };
    if (FPlatformTime::Seconds() - S.WallStarted > 120.)
    { Finish(false, TEXT("Native interactivity checks exceeded 120 seconds")); return; }
    if (!Pawn || !PC || !Selection || !Damage || !Pawn->FirstPersonCamera || !Pawn->FurnitureInteraction || !World->GetGameViewport()) return;
    if (S.Stage != S.LastStage)
    {
        S.LastStage = S.Stage;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_INTERACTIVITY_SMOKE_STAGE stage=%d seconds=%.2f count=%d"),
            S.Stage, Now - S.Started, Damage->GetTotalBrokenCount());
    }
    const float Age = float(Now - S.StageStarted);
    // Targeted contact-damage regression uses only transient impact fixtures.
    const bool bImpactOnly = FParse::Param(FCommandLine::Get(), TEXT("HouseImpactOnly"));
    AHouseInteractiveProp* Target = S.Target.Get();
    if (S.Stage == 0)
    {
        if (Now - S.Started < 3.) return;
        FString UserDir;
        if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || UserDir.IsEmpty())
        { Finish(false, TEXT("Requires isolated UserDir to protect player saves")); return; }
        S.SavedCharacter = Selection->CharacterIndex; S.SavedLoadout = Selection->LoadoutIndex;
        S.SavedGravity = Pawn->GetCharacterMovement()->GravityScale;
        S.SavedWalkSpeed = Pawn->GetCharacterMovement()->MaxWalkSpeed;
        S.bSavedLookIgnored = PC->IsLookInputIgnored(); S.bSavedMoveIgnored = PC->IsMoveInputIgnored(); S.bSavedCursor = PC->bShowMouseCursor;
        if (!bImpactOnly) for (TActorIterator<AHouseInteractiveProp> It(World); It; ++It)
        {
            ++S.InstalledProps;
            if (It->bMovable) ++S.InstalledMovables;
            if (It->bBreakable) ++S.InstalledBreakables;
            if (It->bBreakable && It->BrokenMesh) ++S.InstalledBrokenVariants;
            if (It->Body->IsSimulatingPhysics() || It->bPhysicsActive) ++S.InitiallySimulating;
            if (It->Body->IsSimulatingPhysics() || It->bPhysicsActive || It->IsActorTickEnabled())
            {
                TSharedRef<FJsonObject> State = MakeShared<FJsonObject>();
                State->SetStringField(TEXT("propId"), It->PropId.ToString());
                State->SetStringField(TEXT("actor"), It->GetName());
                State->SetStringField(TEXT("positionCm"), It->GetActorLocation().ToString());
                State->SetBoolField(TEXT("physicsActive"), It->bPhysicsActive);
                State->SetBoolField(TEXT("bodySimulating"), It->Body->IsSimulatingPhysics());
                State->SetBoolField(TEXT("bodyAwake"), It->Body->RigidBodyIsAwake());
                State->SetBoolField(TEXT("tickEnabled"), It->IsActorTickEnabled());
                State->SetBoolField(TEXT("heldTag"), It->Body->ComponentHasTag(TEXT("SSOFurnitureHeld")));
                State->SetNumberField(TEXT("health"), It->Health);
                State->SetNumberField(TEXT("damageStage"), It->DamageStage);
                State->SetNumberField(TEXT("damageEvents"), It->DamageEvents);
                State->SetNumberField(TEXT("breakEvents"), It->BreakEvents);
                S.InitialActivePropReports.Add(MakeShared<FJsonValueObject>(State));
                UE_LOG(LogTemp, Display, TEXT("HOUSE_PROP_INITIAL_ACTIVE id=%s actor=%s active=%d sim=%d awake=%d tick=%d held=%d health=%.2f damageStage=%d damageEvents=%d breaks=%d position=%s"),
                    *It->PropId.ToString(), *It->GetName(), It->bPhysicsActive ? 1 : 0, It->Body->IsSimulatingPhysics() ? 1 : 0,
                    It->Body->RigidBodyIsAwake() ? 1 : 0, It->IsActorTickEnabled() ? 1 : 0,
                    It->Body->ComponentHasTag(TEXT("SSOFurnitureHeld")) ? 1 : 0, It->Health, It->DamageStage,
                    It->DamageEvents, It->BreakEvents, *It->GetActorLocation().ToString());
            }
            if (It->IsActorTickEnabled())
            { Finish(false, FString::Printf(TEXT("Untouched furnishing started a physics-sampling tick: %s"), *It->GetName())); return; }
            if ((!S.Representative.IsValid() || It->PropId == FName(TEXT("SM_GR3_Conversation__003")))
                && !It->bWallMounted && It->bMovable && It->bBreakable && It->IntactMesh && It->BrokenMesh
                && It->Body->Bounds.BoxExtent.GetMax() < 270.f && It->Body->Bounds.BoxExtent.Z > 35.f)
                S.Representative = *It;
            if (It->GetRootComponent() != It->Body || !It->IntactMesh
                || It->Body->GetStaticMesh() != It->IntactMesh || (It->bBreakable && !It->BrokenMesh)
                || (It->bBreakable && It->BreakHealth >= 24.f && (!It->DamagedMesh || !It->HeavilyDamagedMesh)))
            { Finish(false, FString::Printf(TEXT("Installed prop has invalid intact/broken/root setup: %s"), *It->GetName())); return; }
        }
        if (S.InitiallySimulating != 0)
        { Finish(false, TEXT("Untouched installed furnishings started simulating physics")); return; }
        if (FParse::Param(FCommandLine::Get(), TEXT("HouseInteractivityRequireInstalled"))
            && (S.InstalledProps == 0 || !S.Representative.IsValid()))
        { Finish(false, TEXT("Final map check requires installed furniture and a representative broken variant")); return; }
        auto* Floor = World->SpawnActor<AStaticMeshActor>(FVector(400, 0, 7990), FRotator::ZeroRotator);
        Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Floor->SetActorScale3D(FVector(32, 20, .2));
        Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
        S.Transients.Add(Floor);
        S.SavedCameraPostProcess = Pawn->FirstPersonCamera->PostProcessSettings;
        S.SavedCameraPostProcessWeight = Pawn->FirstPersonCamera->PostProcessBlendWeight;
        S.bTestExposureActive = true;
        FPostProcessSettings& Exposure = Pawn->FirstPersonCamera->PostProcessSettings;
        Exposure.bOverride_AutoExposureMethod = true; Exposure.AutoExposureMethod = AEM_Basic;
        Exposure.bOverride_AutoExposureMinBrightness = true; Exposure.AutoExposureMinBrightness = 8.f;
        Exposure.bOverride_AutoExposureMaxBrightness = true; Exposure.AutoExposureMaxBrightness = 8.f;
        Exposure.bOverride_AutoExposureBias = true; Exposure.AutoExposureBias = 0.f;
        Pawn->FirstPersonCamera->PostProcessBlendWeight = 1.f;
        for (const FVector& Position : {FVector(80, -200, 8270), FVector(300, 240, 8320), FVector(580, 0, 8450)})
        {
            if (auto* Light = World->SpawnActor<APointLight>(Position, FRotator::ZeroRotator))
            {
                auto* Bulb = Cast<UPointLightComponent>(Light->GetLightComponent());
                Bulb->SetMobility(EComponentMobility::Movable);
                Bulb->SetIntensityUnits(ELightUnits::Lumens); Bulb->SetIntensity(180000.f);
                Bulb->SetAttenuationRadius(1600.f); Bulb->SetCastShadows(false);
                S.Transients.Add(Light);
            }
        }
        Selection->CharacterIndex = 0; Selection->LoadoutIndex = 1;
        Pawn->ApplySelectedCharacter();
        Pawn->SetActorLocation(FVector(0, 0, 8002.15 + Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        PC->SetControlRotation(FRotator::ZeroRotator);
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
        Stage(1); return;
    }
    if (S.Stage == 1)
    {
        if (Age < .6f || !Pawn->SaucePacket) return;
        Target = PropSmokeSpawn(S, World, FVector(350, 0, Pawn->FirstPersonCamera->GetComponentLocation().Z), FVector(1.5), TEXT("smoke_sauce_target"));
        if (!Target) { Finish(false, TEXT("Could not create transient sauce target")); return; }
        S.Target = Target; S.IdlePosition = Target->GetActorLocation(); S.CountBefore = Damage->GetTotalBrokenCount();
        PropSmokeAim(Pawn, PC, Target->Body->Bounds.Origin);
        Stage(2); return;
    }
    if (S.Stage == 2)
    {
        if (Age < .5f) return;
        if (!Target || Target->Body->IsSimulatingPhysics() || Target->bPhysicsActive
            || FVector::Dist(Target->GetActorLocation(), S.IdlePosition) > .01f)
        { Finish(false, TEXT("Untouched furnishing did not remain parked")); return; }
        S.bIdleStable = true;
        PropSmokeCapture(S, TEXT("intact_before_weapon"));
        Pawn->ServerThrowEquippedWeapon();
        if (!Pawn->SaucePacket || Pawn->SaucePacket->Phase != ESeniorSaucePhase::Flying)
        { Finish(false, TEXT("Native weapon input did not launch Braxton's sauce packet")); return; }
        Stage(3); return;
    }
    if (S.Stage == 3)
    {
        if (Target && Target->bBroken)
        {
            if (Target->Body->GetStaticMesh() != Target->BrokenMesh || Target->BreakEvents != 1
                || Damage->GetTotalBrokenCount() != S.CountBefore + 1)
            { Finish(false, TEXT("Sauce impact did not produce one broken variant and one house-damage count")); return; }
            S.bSauceHit = true;
            PropSmokePointHit(Target, PC, Pawn);
            PropSmokePointHit(Target, PC, Pawn);
            S.bCountOnce = Target->BreakEvents == 1 && Damage->GetTotalBrokenCount() == S.CountBefore + 1;
            if (!S.bCountOnce) { Finish(false, TEXT("A previously broken piece counted damage twice")); return; }
            Stage(4); return;
        }
        if (Age > 3.f) Finish(false, TEXT("Real sauce flight failed to break the target through the weapon filter"));
        return;
    }
    if (S.Stage == 4)
    {
        if (Age < .7f) return;
        PropSmokeAim(Pawn, PC, Target->Body->Bounds.Origin);
        PropSmokeCapture(S, TEXT("broken_after_sauce"));
        Stage(41); return;
    }
    if (S.Stage == 41)
    {
        if (Age < .25f || FScreenshotRequest::IsScreenshotRequested()) return;
        PropSmokeRemove(S.Target);
        Selection->LoadoutIndex = 0;
        Stage(5); return;
    }
    if (S.Stage == 5)
    {
        if (Age < .7f || !Pawn->Douli || Pawn->Douli->Phase != EDouliPhase::Ready) return;
        Target = PropSmokeSpawn(S, World, FVector(350, 0, Pawn->FirstPersonCamera->GetComponentLocation().Z), FVector(1.5), TEXT("smoke_douli_target"));
        if (!Target) { Finish(false, TEXT("Could not create transient douli target")); return; }
        S.Target = Target; S.CountBefore = Damage->GetTotalBrokenCount();
        PropSmokeAim(Pawn, PC, Target->Body->Bounds.Origin);
        Stage(6); return;
    }
    if (S.Stage == 6)
    {
        if (Age < .15f) return;
        Pawn->ServerThrowEquippedWeapon();
        if (!Pawn->Douli || Pawn->Douli->Phase != EDouliPhase::Windup)
        { Finish(false, TEXT("Native weapon input did not start the douli throw")); return; }
        Stage(7); return;
    }
    if (S.Stage == 7)
    {
        if (Target && Target->bBroken)
        {
            if (Target->Body->GetStaticMesh() != Target->BrokenMesh || Target->BreakEvents != 1
                || Damage->GetTotalBrokenCount() != S.CountBefore + 1)
            { Finish(false, TEXT("Douli impact did not switch the visible mesh and count once")); return; }
            S.bDouliHit = true; Stage(8); return;
        }
        if (Age > 3.f) Finish(false, TEXT("Real douli flight failed to break the target through the weapon filter"));
        return;
    }
    if (S.Stage == 8)
    {
        if (Age < .7f || !Pawn->Douli || Pawn->Douli->Phase != EDouliPhase::Ready) return;
        PropSmokeRemove(S.Target);
        Target = PropSmokeSpawn(S, World, FVector(185, 0, Pawn->FirstPersonCamera->GetComponentLocation().Z - 25), FVector(.7), TEXT("smoke_held_break"));
        if (!Target) { Finish(false, TEXT("Could not create held-break furnishing")); return; }
        S.Target = Target; S.CountBefore = Damage->GetTotalBrokenCount();
        PropSmokeAim(Pawn, PC, Target->Body->Bounds.Origin);
        Stage(9); return;
    }
    if (S.Stage == 9)
    {
        if (Age < .15f) return;
        if (Pawn->FurnitureInteraction->FocusedFurnitureForInteraction() != Target)
        { Finish(false, TEXT("Parked furniture was not focusable before waking physics")); return; }
        Pawn->FurnitureInteraction->ToggleGrab();
        if (!Pawn->FurnitureInteraction->bHoldingFurniture || !Target->bPhysicsActive || !Target->Body->IsSimulatingPhysics())
        { Finish(false, TEXT("Native E furniture grab did not wake and hold the parked body")); return; }
        Stage(10); return;
    }
    if (S.Stage == 10)
    {
        if (Age < .55f) return;
        if (!Pawn->FurnitureInteraction->bHoldingFurniture)
        { Finish(false, TEXT("Furniture handle released before the deliberate damage test")); return; }
        PropSmokePointHit(Target, PC, Pawn);
        Stage(11); return;
    }
    if (S.Stage == 11)
    {
        if (Age < .15f) return;
        S.bHeldRelease = Target && Target->bBroken && !Pawn->FurnitureInteraction->bHoldingFurniture
            && !Target->Body->ComponentHasTag(TEXT("SSOFurnitureHeld"))
            && Damage->GetTotalBrokenCount() == S.CountBefore + 1;
        if (!S.bHeldRelease) { Finish(false, TEXT("Breaking a held body left the grab reserved or failed to count")); return; }
        PropSmokeRemove(S.Target);
        Target = PropSmokeSpawn(S, World, FVector(485, 0, Pawn->FirstPersonCamera->GetComponentLocation().Z - 20), FVector(1.2, 3, 3), TEXT("smoke_thrown_target"), false);
        AHouseInteractiveProp* Thrown = PropSmokeSpawn(S, World, FVector(185, 0, Pawn->FirstPersonCamera->GetComponentLocation().Z - 20), FVector(.55), TEXT("smoke_thrown_piece"));
        if (!Target || !Thrown) { Finish(false, TEXT("Could not create thrown-furniture fixtures")); return; }
        S.Target = Target; S.Thrown = Thrown; S.CountBefore = Damage->GetTotalBrokenCount();
        PropSmokeAim(Pawn, PC, Thrown->Body->Bounds.Origin);
        Stage(12); return;
    }
    if (S.Stage == 12)
    {
        if (Age < .15f) return;
        Pawn->FurnitureInteraction->ToggleGrab();
        if (!Pawn->FurnitureInteraction->bHoldingFurniture)
        { Finish(false, TEXT("Could not grab the throwable furniture piece")); return; }
        Stage(13); return;
    }
    if (S.Stage == 13)
    {
        AHouseInteractiveProp* Thrown = S.Thrown.Get();
        UHouseFurnitureInteractionComponent* Mover = Pawn->FurnitureInteraction;
        if (!Target || !Thrown || !Mover->bHoldingFurniture || Mover->GetHeldBody() != Thrown->Body)
        { Finish(false, TEXT("Throw fixture released before pickup completed")); return; }
        // Keep looking at the intended target while the physical lift finishes.
        // The old fixed .55-second delay fired during the reach with stale aim.
        PropSmokeAim(Pawn, PC, Target->Body->Bounds.Origin);
        FVector Right, Left, Normal;
        const bool bRight = Mover->GetHandContact(true, Right, Normal);
        const bool bLeft = Mover->GetHandContact(false, Left, Normal);
        const float RightError = bRight ? FVector::Dist(Right, Pawn->GetFurnitureHandGripPoint(true)) : BIG_NUMBER;
        const float LeftError = bLeft ? FVector::Dist(Left, Pawn->GetFurnitureHandGripPoint(false)) : BIG_NUMBER;
        const bool bReady = Age >= 1.15f && Mover->GetCarryPoseBlend() >= .985f
            && Mover->GetFingerGripBlend() >= .95f && RightError <= 8.f
            && (!Mover->UsesTwoHands() || LeftError <= 8.f)
            && Thrown->Body->GetPhysicsLinearVelocity().Size() < 80.f;
        if (!bReady) S.ThrowHoldReadySince = -1.;
        else if (S.ThrowHoldReadySince < 0.) S.ThrowHoldReadySince = Now;
        if (S.ThrowHoldReadySince >= 0. && Now - S.ThrowHoldReadySince >= .12)
        {
            S.ThrowStartReport = MakeShared<FJsonObject>();
            S.ThrowStartReport->SetNumberField(TEXT("pickupSeconds"), Age);
            S.ThrowStartReport->SetNumberField(TEXT("rightPalmErrorCm"), RightError);
            S.ThrowStartReport->SetNumberField(TEXT("leftPalmErrorCm"), LeftError);
            S.ThrowStartReport->SetNumberField(TEXT("carryPoseBlend"), Mover->GetCarryPoseBlend());
            S.ThrowStartReport->SetNumberField(TEXT("fingerGripBlend"), Mover->GetFingerGripBlend());
            Stage(131); return;
        }
        if (Age > 2.6f)
            Finish(false, FString::Printf(TEXT("Throw pickup did not finish: pose %.3f grip %.3f palms %.2f/%.2f cm speed %.2f"),
                Mover->GetCarryPoseBlend(), Mover->GetFingerGripBlend(), RightError, LeftError,
                Thrown->Body->GetPhysicsLinearVelocity().Size()));
        return;
    }
    if (S.Stage == 131)
    {
        if (Age < .06f) return; // Render/animation receives the final aimed hold.
        AHouseInteractiveProp* Thrown = S.Thrown.Get();
        if (!Thrown || !Target || !Pawn->FurnitureInteraction->bHoldingFurniture)
        { Finish(false, TEXT("Completed throw grip vanished before native input")); return; }
        PropSmokeAim(Pawn, PC, Target->Body->Bounds.Origin);
        const FVector View = Pawn->FirstPersonCamera->GetComponentLocation();
        const FVector Direction = PC->GetControlRotation().Vector();
        FHitResult AimHit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(HousePropSmokeThrowAim), false);
        Query.AddIgnoredActor(Pawn); Query.AddIgnoredActor(Thrown);
        const bool bAimAtTarget = World->LineTraceSingleByChannel(AimHit, View, View + Direction * 1500.f, ECC_Visibility, Query)
            && AimHit.GetActor() == Target;
        S.ThrowStartReport->SetBoolField(TEXT("viewRayHitsTarget"), bAimAtTarget);
        S.ThrowStartReport->SetStringField(TEXT("viewPositionCm"), View.ToString());
        S.ThrowStartReport->SetStringField(TEXT("viewDirection"), Direction.ToString());
        S.ThrowStartReport->SetStringField(TEXT("bodyPositionCm"), Thrown->Body->Bounds.Origin.ToString());
        S.ThrowStartReport->SetStringField(TEXT("bodyExtentsCm"), Thrown->Body->Bounds.BoxExtent.ToString());
        S.ThrowStartReport->SetNumberField(TEXT("impactBreakSpeedCmPerSecond"), Thrown->ImpactBreakSpeed);
        S.ThrowStartReport->SetStringField(TEXT("preReleaseVelocityCmPerSecond"), Thrown->Body->GetPhysicsLinearVelocity().ToString());
        S.ThrowStartReport->SetStringField(TEXT("targetPositionCm"), Target->Body->Bounds.Origin.ToString());
        S.ThrowStartReport->SetStringField(TEXT("targetExtentsCm"), Target->Body->Bounds.BoxExtent.ToString());
        S.ThrowStartReport->SetNumberField(TEXT("targetHealth"), Target->Health);
        if (!bAimAtTarget) { Finish(false, TEXT("Native throw setup is not aimed at its target")); return; }
        S.ThrowReleasedAt = Now;
        Pawn->ServerThrowEquippedWeapon();
        if (Pawn->FurnitureInteraction->bHoldingFurniture)
        { Finish(false, TEXT("Native fire action did not release held furniture for its throw")); return; }
        S.ThrowStartReport->SetStringField(TEXT("launchVelocityCmPerSecond"), Thrown->Body->GetPhysicsLinearVelocity().ToString());
        S.ThrowStartReport->SetBoolField(TEXT("heldTagCleared"), !Thrown->Body->ComponentHasTag(TEXT("SSOFurnitureHeld")));
        UE_LOG(LogTemp, Display, TEXT("HOUSE_PROP_THROW_START position=%s velocity=%s target=%s aim=%s"),
            *Thrown->Body->Bounds.Origin.ToString(), *Thrown->Body->GetPhysicsLinearVelocity().ToString(),
            *Target->Body->Bounds.Origin.ToString(), *Direction.ToString());
        PropSmokeRecordThrowFlight(S, Now, TEXT("launch"));
        Stage(14); return;
    }
    if (S.Stage == 14)
    {
        PropSmokeRecordThrowFlight(S, Now, TEXT("flight"));
        if (Target && Target->bBroken)
        {
            S.bThrownImpact = S.ThrowSpeed > 400.f && Target->BreakEvents == 1
                && Target->Body->GetStaticMesh() == Target->BrokenMesh && Damage->GetTotalBrokenCount() > S.CountBefore;
            if (!S.bThrownImpact) { Finish(false, TEXT("Thrown furniture impact lacked a real launch or broken target")); return; }
            if (bImpactOnly)
            {
                Finish(true, TEXT("Targeted impact regression passed: native sauce, douli, held break release and actual furniture throw. Installed asset audit and later fixture checks were not requested."));
                return;
            }
            Stage(15); return;
        }
        if (Age > 3.f) Finish(false, FString::Printf(
            TEXT("Native thrown prop did not break target: closest center-to-surface %.2f cm, target health %.2f, stage %d. Inspect launch and flight diagnostics."),
            S.ThrowClosestTargetCm, Target ? Target->Health : -1.f, Target ? Target->DamageStage : -1));
        return;
    }
    if (S.Stage == 15)
    {
        if (Age < .4f) return;
        PropSmokeRemove(S.Target); PropSmokeRemove(S.Thrown);
        AHouseInteractiveProp* Original = S.Representative.Get();
        if (!Original) { Stage(50); return; }
        S.RepresentativeName = Original->PropName;
        const FTransform Transform(Original->GetActorRotation(), FVector(270, 0, 8150), Original->GetActorScale3D());
        Target = World->SpawnActorDeferred<AHouseInteractiveProp>(AHouseInteractiveProp::StaticClass(), Transform,
            nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Target) { Finish(false, TEXT("Could not instantiate representative authored assets")); return; }
        Target->IntactMesh = Original->IntactMesh; Target->BrokenMesh = Original->BrokenMesh;
        Target->DamagedMesh = Original->DamagedMesh; Target->HeavilyDamagedMesh = Original->HeavilyDamagedMesh;
        Target->BreakHealth = Original->BreakHealth;
        Target->MaterialOverrides = Original->MaterialOverrides;
        // Exercise actual instance overrides even when the installed actor
        // relies on its mesh defaults. These are the same intact materials.
        const TArray<FStaticMaterial>& OriginalSlots = Original->IntactMesh->GetStaticMaterials();
        Target->MaterialOverrides.SetNum(OriginalSlots.Num());
        for (int32 Index = 0; Index < OriginalSlots.Num(); ++Index)
            if (!Target->MaterialOverrides[Index]) Target->MaterialOverrides[Index] = OriginalSlots[Index].MaterialInterface;
        Target->PropName = Original->PropName; Target->PropId = TEXT("smoke_authored_variant");
        Target->bMovable = true; Target->bBrokenVersionMovable = true; Target->MassKg = Original->MassKg;
        Target->FinishSpawning(Transform);
        Target->SetActorLocation(Target->GetActorLocation() + FVector(0, 0, 8001 - PropSmokeVisibleBottom(Target->Body)));
        S.Transients.Add(Target); S.Target = Target; S.CountBefore = Damage->GetTotalBrokenCount();
        PropSmokeAim(Pawn, PC, Target->Body->Bounds.Origin);
        Stage(16); return;
    }
    if (S.Stage == 16)
    {
        if (Age < .4f) return;
        PropSmokeCapture(S, TEXT("authored_intact_variant"));
        Stage(17); return;
    }
    if (S.Stage == 17)
    {
        if (Age < .3f) return;
        if (!Target->DamagedMesh || !Target->HeavilyDamagedMesh)
        { Finish(false, TEXT("Representative authored furnishing has no progressive damage variants")); return; }
        PropSmokePointHit(Target, PC, Pawn, Target->BreakHealth / 3.f);
        Stage(171); return;
    }
    if (S.Stage == 171)
    {
        FString Failure;
        const int32 Landing = PropSmokeGrounding(S, Target, Now, Age, TEXT("isolated platform"), Failure);
        if (Landing < 0) { Finish(false, Failure); return; }
        if (Landing == 0) return;
        if (Target->DamageStage != 1 || Target->Body->GetStaticMesh() != Target->DamagedMesh
            || Target->bBroken || Damage->GetTotalBrokenCount() != S.CountBefore)
        { Finish(false, TEXT("Authored furniture did not show the first damage stage without counting a break")); return; }
        PropSmokeCapture(S, TEXT("authored_damaged_variant"));
        Stage(172); return;
    }
    if (S.Stage == 172)
    {
        if (Age < .25f || FScreenshotRequest::IsScreenshotRequested()) return;
        PropSmokeAim(Pawn, PC, Target->Body->Bounds.Origin);
        Pawn->FurnitureInteraction->ToggleGrab();
        if (!Pawn->FurnitureInteraction->bHoldingFurniture)
        { Finish(false, TEXT("Could not grab the actual authored damaged furnishing before the next stage")); return; }
        Stage(175); return;
    }
    if (S.Stage == 175)
    {
        if (Age < .45f) return;
        if (!Pawn->FurnitureInteraction->bHoldingFurniture)
        { Finish(false, TEXT("Actual authored furnishing could not remain held for the stage-change test")); return; }
        PropSmokePointHit(Target, PC, Pawn, Target->BreakHealth / 3.f);
        S.bAuthoredHeldStageReleased = !Pawn->FurnitureInteraction->bHoldingFurniture
            && !Target->Body->ComponentHasTag(TEXT("SSOFurnitureHeld"));
        if (!S.bAuthoredHeldStageReleased)
        { Finish(false, TEXT("Authored damage-stage body retained a stale hold constraint")); return; }
        Stage(173); return;
    }
    if (S.Stage == 173)
    {
        FString Failure;
        const int32 Landing = PropSmokeGrounding(S, Target, Now, Age, TEXT("platform after held stage swap"), Failure);
        if (Landing < 0) { Finish(false, Failure); return; }
        if (Landing == 0) return;
        if (Target->DamageStage != 2 || Target->Body->GetStaticMesh() != Target->HeavilyDamagedMesh
            || Target->bBroken || Damage->GetTotalBrokenCount() != S.CountBefore)
        { Finish(false, TEXT("Authored furniture did not show the heavy damage stage without counting a break")); return; }
        S.bAuthoredDamageStages = true;
        PropSmokeCapture(S, TEXT("authored_heavily_damaged_variant"));
        Stage(174); return;
    }
    if (S.Stage == 174)
    {
        if (Age < .25f || FScreenshotRequest::IsScreenshotRequested()) return;
        PropSmokePointHit(Target, PC, Pawn, Target->Health);
        Stage(18); return;
    }
    if (S.Stage == 18)
    {
        FString Failure;
        const int32 Landing = PropSmokeGrounding(S, Target, Now, Age, TEXT("platform broken remains"), Failure);
        if (Landing < 0) { Finish(false, Failure); return; }
        if (Landing == 0) return;
        S.bAuthoredVariant = Target && Target->bBroken && Target->Body->GetStaticMesh() == Target->BrokenMesh
            && Target->BrokenMesh != Target->IntactMesh && Damage->GetTotalBrokenCount() == S.CountBefore + 1;
        if (!S.bAuthoredVariant) { Finish(false, TEXT("The actual installed furniture assets did not produce the configured destroyed version")); return; }
        S.bAuthoredStagesSettled = true;
        PropSmokeCapture(S, TEXT("authored_broken_variant"));
        Stage(50); return;
    }
    if (S.Stage == 50)
    {
        if (Age < .3f || FScreenshotRequest::IsScreenshotRequested()) return;
        PropSmokeRemove(S.Target);
        Target = PropSmokeSpawn(S, World, FVector(350, 0, Pawn->FirstPersonCamera->GetComponentLocation().Z),
            FVector(.15, 1.2, 1.2), TEXT("smoke_wall_picture"), false);
        if (!Target) { Finish(false, TEXT("Could not create wall-picture fixture")); return; }
        Target->bWallMounted = true; Target->RefreshProp();
        S.Target = Target; S.IdlePosition = Target->GetActorLocation(); S.CountBefore = Damage->GetTotalBrokenCount();
        PropSmokeAim(Pawn, PC, Target->Body->Bounds.Origin);
        Stage(51); return;
    }
    if (S.Stage == 51)
    {
        if (Age < .15f || !Pawn->Douli || Pawn->Douli->Phase != EDouliPhase::Ready) return;
        Pawn->ServerThrowEquippedWeapon(); Stage(52); return;
    }
    if (S.Stage == 52)
    {
        if (Target && Target->IsDetached())
        {
            if (Target->bBroken || Target->Body->GetStaticMesh() != Target->IntactMesh
                || !Target->Body->IsSimulatingPhysics() || Damage->GetTotalBrokenCount() != S.CountBefore + 1)
            { Finish(false, TEXT("First weapon hit did not unhook the intact wall picture and count once")); return; }
            Stage(53); return;
        }
        if (Age > 3.f) Finish(false, TEXT("Real douli flight failed to knock a wall picture loose"));
        return;
    }
    if (S.Stage == 53)
    {
        if (Age < .24f) return;
        S.WallFallDistance = S.IdlePosition.Z - Target->GetActorLocation().Z;
        if (Target->bBroken || S.WallFallDistance < 1.f || !Target->CanBeMoved())
        { Finish(false, TEXT("Unhooked picture did not fall intact under gravity")); return; }
        PropSmokeAim(Pawn, PC, Target->Body->Bounds.Origin);
        PropSmokeCapture(S, TEXT("picture_knocked_off_wall"));
        Stage(54); return;
    }
    if (S.Stage == 54)
    {
        if (Age < .2f || FScreenshotRequest::IsScreenshotRequested()) return;
        PropSmokePointHit(Target, PC, Pawn); Stage(55); return;
    }
    if (S.Stage == 55)
    {
        if (Age < .15f) return;
        S.bWallDetached = Target && Target->bDetachedFromWall && Target->bBroken
            && Target->Body->GetStaticMesh() == Target->BrokenMesh
            && Damage->GetTotalBrokenCount() == S.CountBefore + 1;
        if (!S.bWallDetached) { Finish(false, TEXT("Detached picture's second hit failed to break it without double counting")); return; }
        PropSmokeRemove(S.Target);
        Target = PropSmokeSpawn(S, World, FVector(300, 500, 8060), FVector(2, 2, 1.2), TEXT("smoke_support_table"));
        auto* Cup = PropSmokeSpawn(S, World, FVector(300, 500, 8140), FVector(.4), TEXT("smoke_supported_cup"));
        if (!Target || !Cup) { Finish(false, TEXT("Could not create support-and-contents fixtures")); return; }
        S.Target = Target; S.Thrown = Cup; S.IdlePosition = Cup->GetActorLocation();
        if (!Target->WakeForInteraction(Pawn) || !Cup->bPhysicsActive || !Cup->Body->IsSimulatingPhysics())
        { Finish(false, TEXT("Moving the supporting table did not wake its resting contents")); return; }
        Target->Body->SetPhysicsLinearVelocity(FVector(700, 0, 0));
        Stage(56); return;
    }
    if (S.Stage == 56)
    {
        // Keep the test table moving long enough to actually clear its contents.
        // A single velocity pulse can slow through friction and carry the cup;
        // that valid physics response does not demonstrate a removed support.
        if (Target && Age < .7f) Target->Body->SetPhysicsLinearVelocity(FVector(700, 0, 0));
        if (Age < 1.2f) return;
        if (S.Thrown.IsValid() && Target)
        {
            S.SupportedCupFall = S.IdlePosition.Z - S.Thrown->GetActorLocation().Z;
            S.SupportSeparation = FMath::Abs(Target->GetActorLocation().X - S.Thrown->GetActorLocation().X);
            UE_LOG(LogTemp, Display, TEXT("HOUSE_SUPPORT_SMOKE table=%s cup=%s separation=%.2f cupFall=%.2f simulating=%d"),
                *Target->GetActorLocation().ToCompactString(), *S.Thrown->GetActorLocation().ToCompactString(),
                S.SupportSeparation, S.SupportedCupFall, S.Thrown->Body->IsSimulatingPhysics() ? 1 : 0);
        }
        S.bSupportedPropFell = S.Thrown.IsValid() && S.Thrown->bPhysicsActive
            && S.SupportSeparation > 125.f && S.SupportedCupFall > 20.f;
        if (!S.bSupportedPropFell) { Finish(false, TEXT("Supported contents did not fall after the driven table cleared them")); return; }
        PropSmokeRemove(S.Target); PropSmokeRemove(S.Thrown);
        Stage(57); return;
    }
    if (S.Stage == 57)
    {
        Target = PropSmokeSpawn(S, World, FVector(300, 500, 8060), FVector(2, 2, 1.2), TEXT("smoke_sturdy_piece"), false);
        if (!Target) { Finish(false, TEXT("Could not create sturdy furnishing fixture")); return; }
        Target->BreakHealth = 32.f; Target->Health = 32.f;
        S.Target = Target; S.CountBefore = Damage->GetTotalBrokenCount();
        PropSmokePointHit(Target, PC, Pawn);
        S.bSturdyPieceResisted = !Target->bBroken && Target->Health == 24.f
            && Target->Body->GetStaticMesh() == Target->IntactMesh && Damage->GetTotalBrokenCount() == S.CountBefore;
        if (!S.bSturdyPieceResisted) { Finish(false, TEXT("Sturdy furniture did not resist the first weak impact")); return; }
        Stage(58); return;
    }
    if (S.Stage == 58)
    {
        if (Age < .15f) return;
        for (int32 Hit = 0; Hit < 3; ++Hit) PropSmokePointHit(Target, PC, Pawn);
        if (!Target->bBroken || Damage->GetTotalBrokenCount() != S.CountBefore + 1)
        { Finish(false, TEXT("Repeated impacts did not eventually break sturdy furniture once")); return; }
        PropSmokeRemove(S.Target);
        Target = PropSmokeSpawn(S, World, FVector(350, 500, 8200), FVector(.2, 1.5, 1.5), TEXT("smoke_sturdy_wall_mount"), false);
        if (!Target) { Finish(false, TEXT("Could not create sturdy wall-mount fixture")); return; }
        Target->bWallMounted = true; Target->DetachHealth = 32.f; Target->AttachmentHealth = 32.f; Target->RefreshProp();
        S.Target = Target; S.CountBefore = Damage->GetTotalBrokenCount();
        PropSmokePointHit(Target, PC, Pawn);
        S.bSturdyMountResisted = !Target->IsDetached() && !Target->bPhysicsActive
            && Target->AttachmentHealth == 24.f && Damage->GetTotalBrokenCount() == S.CountBefore;
        if (!S.bSturdyMountResisted) { Finish(false, TEXT("Large wall mount did not resist the first weak impact")); return; }
        Stage(59); return;
    }
    if (S.Stage == 59)
    {
        if (Age < .15f) return;
        for (int32 Hit = 0; Hit < 3; ++Hit) PropSmokePointHit(Target, PC, Pawn);
        if (!Target->IsDetached() || Target->bBroken || !Target->Body->IsSimulatingPhysics()
            || Damage->GetTotalBrokenCount() != S.CountBefore + 1)
        { Finish(false, TEXT("Repeated impacts did not detach the large wall mount intact")); return; }
        PropSmokeRemove(S.Target);
        Stage(60); return;
    }
    if (S.Stage == 60)
    {
        Target = PropSmokeSpawn(S, World, FVector(185, 0, Pawn->FirstPersonCamera->GetComponentLocation().Z - 25), FVector(.7), TEXT("smoke_damage_stages"));
        if (!Target) { Finish(false, TEXT("Could not create progressive-damage fixture")); return; }
        Target->BreakHealth = 24.f; Target->Health = 24.f;
        Target->DamagedMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
        Target->HeavilyDamagedMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
        Target->RefreshProp();
        S.Target = Target; S.CountBefore = Damage->GetTotalBrokenCount();
        PropSmokeAim(Pawn, PC, Target->Body->Bounds.Origin);
        Stage(61); return;
    }
    if (S.Stage == 61)
    {
        if (Age < .15f) return;
        Pawn->FurnitureInteraction->ToggleGrab();
        if (!Pawn->FurnitureInteraction->bHoldingFurniture)
        { Finish(false, TEXT("Could not hold the staged-damage furnishing")); return; }
        Stage(62); return;
    }
    if (S.Stage == 62)
    {
        if (Age < .3f) return;
        PropSmokePointHit(Target, PC, Pawn);
        if (Target->bBroken || Target->DamageStage != 1 || Target->Health != 16.f
            || Target->Body->GetStaticMesh() != Target->DamagedMesh || Pawn->FurnitureInteraction->bHoldingFurniture
            || Target->Body->ComponentHasTag(TEXT("SSOFurnitureHeld")) || Damage->GetTotalBrokenCount() != S.CountBefore)
        { Finish(false, TEXT("First nonfatal damage stage failed to swap mesh and safely release its physics handle")); return; }
        Stage(63); return;
    }
    if (S.Stage == 63)
    {
        if (Age < .15f) return;
        PropSmokePointHit(Target, PC, Pawn);
        if (Target->bBroken || Target->DamageStage != 2 || Target->Health != 8.f
            || Target->Body->GetStaticMesh() != Target->HeavilyDamagedMesh || Damage->GetTotalBrokenCount() != S.CountBefore)
        { Finish(false, TEXT("Second nonfatal hit failed to show heavy damage without counting destruction")); return; }
        Stage(64); return;
    }
    if (S.Stage == 64)
    {
        if (Age < .15f) return;
        PropSmokePointHit(Target, PC, Pawn);
        S.bDamageStages = Target->bBroken && Target->DamageStage == 3 && Target->Health == 0.f
            && Target->Body->GetStaticMesh() == Target->BrokenMesh && Damage->GetTotalBrokenCount() == S.CountBefore + 1;
        if (!S.bDamageStages) { Finish(false, TEXT("Final damage stage failed to show destruction and count once")); return; }
        const float Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const float HalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const FVector Center(Target->Body->Bounds.Origin.X, Target->Body->Bounds.Origin.Y,
            FMath::Max(8005.f + HalfHeight, float(Target->Body->Bounds.Origin.Z)));
        const FVector Travel(Target->Body->Bounds.BoxExtent.X + Radius + 24.f, 0, 0);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(BrokenFurniturePlayerPass), false, Pawn);
        FHitResult Blocked, Clear;
        const bool bPawnIgnored = Target->Body->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore;
        const bool bFloorSolid = Target->Body->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Block;
        const bool bCanTarget = Target->Body->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block;
        // A positive control proves this is a real path through the body, then
        // the actual destroyed response must allow the same full-size capsule.
        Target->Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        const bool bWouldBlock = World->SweepSingleByChannel(Blocked, Center - Travel, Center + Travel,
            FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Query) && Blocked.GetActor() == Target;
        Target->Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        const bool bStillBlocks = World->SweepSingleByChannel(Clear, Center - Travel, Center + Travel,
            FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Query);
        S.bBrokenCapsulePass = bPawnIgnored && bFloorSolid && bCanTarget && bWouldBlock && !bStillBlocks;
        if (!S.bBrokenCapsulePass) { Finish(false, TEXT("Destroyed furniture did not permit the actual player capsule while retaining floor and pickup collision")); return; }
        PropSmokeRemove(S.Target);
        Stage(65); return;
    }
    if (S.Stage == 65)
    {
        Target = S.Representative.Get();
        if (!Target) { Stage(19); return; }
        const FBox Bounds = Target->Body->Bounds.GetBox();
        const FVector Bottom(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseInstalledPropGround), false, Target);
        FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_WorldStatic);
        FHitResult Floor;
        if (!World->LineTraceSingleByObjectType(Floor, Bottom + FVector(0, 0, 8), Bottom - FVector(0, 0, 100), Objects, Query))
        { Finish(false, TEXT("No actual house floor found under representative installed furniture")); return; }
        S.GroundFloorZ = Floor.ImpactPoint.Z;
        S.Target = Target; S.CountBefore = Damage->GetTotalBrokenCount();
        PropSmokePointHit(Target, PC, Pawn, Target->BreakHealth / 3.f);
        Stage(66); return;
    }
    if (S.Stage == 66 || S.Stage == 67 || S.Stage == 68)
    {
        FString Failure;
        const int32 Landing = PropSmokeGrounding(S, Target, Now, Age, TEXT("actual installed house position"), Failure);
        if (Landing < 0) { Finish(false, Failure); return; }
        if (Landing == 0) return;
        const int32 ExpectedDamageStage = S.Stage - 65;
        if (Target->DamageStage != ExpectedDamageStage)
        { Finish(false, TEXT("Installed furnishing skipped a damage stage while settling in the house")); return; }
        if (S.Stage < 68)
        {
            if (Damage->GetTotalBrokenCount() != S.CountBefore)
            { Finish(false, TEXT("In-house intermediate damage stage incorrectly counted destruction")); return; }
            PropSmokePointHit(Target, PC, Pawn, S.Stage == 67 ? Target->Health : Target->BreakHealth / 3.f);
            Stage(S.Stage + 1); return;
        }
        S.bInHouseStagesSettled = Target->bBroken && Target->Body->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore
            && Damage->GetTotalBrokenCount() == S.CountBefore + 1;
        if (!S.bInHouseStagesSettled)
        { Finish(false, TEXT("In-house final damage stage failed collision/count checks")); return; }
        // Observe only after physics checks have passed. A temporary camera
        // avoids moving the player capsule into nearby approved furniture.
        const FVector LookAt = Target->Body->Bounds.Origin;
        const FVector ViewPosition = LookAt + FVector(230.f, -230.f, 175.f);
        if (ACameraActor* Camera = World->SpawnActor<ACameraActor>(ViewPosition, (LookAt - ViewPosition).Rotation()))
        {
            Camera->GetCameraComponent()->PostProcessSettings = S.SavedCameraPostProcess;
            Camera->GetCameraComponent()->PostProcessBlendWeight = S.SavedCameraPostProcessWeight;
            Camera->GetCameraComponent()->FieldOfView = 70.f;
            S.SavedObservationViewTarget = PC->GetViewTarget();
            S.Transients.Add(Camera);
            PC->SetViewTarget(Camera);
            Stage(690); return;
        }
        // This was the existing map actor in an isolated test world, not a saved edit.
        S.Target.Reset();
        Stage(19); return;
    }
    if (S.Stage == 690)
    {
        if (Age < .2f) return;
        PropSmokeCapture(S, TEXT("actual_house_broken_chair_settled"));
        Stage(691); return;
    }
    if (S.Stage == 691)
    {
        if (Age < .1f || FScreenshotRequest::IsScreenshotRequested()) return;
        if (S.SavedObservationViewTarget.IsValid()) PC->SetViewTarget(S.SavedObservationViewTarget.Get());
        S.SavedObservationViewTarget.Reset();
        S.Target.Reset();
        Stage(19); return;
    }
    if (S.Stage == 19)
    {
        if (Age < .3f) return;
        if (S.bTestExposureActive)
        {
            Pawn->FirstPersonCamera->PostProcessSettings = S.SavedCameraPostProcess;
            Pawn->FirstPersonCamera->PostProcessBlendWeight = S.SavedCameraPostProcessWeight;
            S.bTestExposureActive = false;
        }
        PropSmokeRemove(S.Target);
        FString Failure;
        if (!TickHouseFixtureSmoke(World, Pawn, PC, Failure, S.FixtureReport)) return;
        if (!Failure.IsEmpty()) { Finish(false, Failure); return; }
        Stage(20); return;
    }
    if (S.Stage == 20)
    {
        if (Age < .3f || FScreenshotRequest::IsScreenshotRequested()) return;
        if (Pawn->GetCharacterMovement()->GravityScale != S.SavedGravity
            || PC->IsLookInputIgnored() != S.bSavedLookIgnored || PC->IsMoveInputIgnored() != S.bSavedMoveIgnored
            || PC->bShowMouseCursor != S.bSavedCursor)
        { Finish(false, TEXT("Furniture interaction changed player input or gravity")); return; }
        for (TActorIterator<AHouseInteractiveProp> It(World); It; ++It)
        {
            if (S.Transients.Contains(*It)) continue;
            if (S.bInHouseStagesSettled && *It == S.Representative.Get()) continue;
            if (It->bPhysicsActive || It->bBroken)
            { Finish(false, TEXT("A protected authored furnishing changed during isolated checks")); return; }
        }
        Finish(true, TEXT("Native weapon hits, movable/throwable furniture, held damage release, authored stage landing on an isolated floor and in the house, one-count house damage, parked bodies and reactive fixtures passed."));
    }
}
#endif

void TickHouseInteractivitySmoke(UWorld* World)
{
#if !UE_BUILD_SHIPPING
    RunHouseInteractivitySmoke(World);
#endif
}
