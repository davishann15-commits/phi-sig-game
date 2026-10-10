// Opt-in, isolated Development review of real campaign pickups and worn masks.
// This file does not alter normal gameplay and does not save the house map.
#include "StoryCampaign.h"
#include "HouseOxbloodMask.h"
#include "SeniorBraxtonVisual.h"
#include "SeniorRunnerVisual.h"
#include "SeniorFixerVisual.h"

#if !UE_BUILD_SHIPPING
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "CoreGlobals.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Scalability.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StaticMeshResources.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif

namespace
{
constexpr uint64 MaskWarmupFrames = 90;
constexpr double MaskWarmupSeconds = 3.0;

struct FMaskReview
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;
    TWeakObjectPtr<ACameraActor> Camera;
    TArray<TWeakObjectPtr<AHouseOxbloodMaskPickup>> Pickups;
    TArray<TSharedPtr<FJsonValue>> PlacementResults, WearingResults, Captures;
    FString Directory, Pending, ShotName;
    FVector OriginalLocation = FVector::ZeroVector;
    FRotator OriginalRotation = FRotator::ZeroRotator;
    int32 Stage = 0, PickupIndex = 0, CharacterIndex = 0, Side = 0;
    double Started = 0, Changed = 0, Requested = 0;
    uint64 ChangedFrame = 0;
    bool bFinished = false, bCollected = false, bRemovalPassed = false, bReequipPassed = false, bPlacementsPassed = true;
};

TSharedRef<FJsonObject> MaskPoint(const FVector& V)
{
    TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
    O->SetNumberField(TEXT("x"), V.X); O->SetNumberField(TEXT("y"), V.Y); O->SetNumberField(TEXT("z"), V.Z);
    return O;
}

TSharedRef<FJsonObject> MaskTransform(const FTransform& T)
{
    TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
    O->SetObjectField(TEXT("location"), MaskPoint(T.GetLocation()));
    O->SetObjectField(TEXT("rotationPitchYawRoll"), MaskPoint(FVector(T.Rotator().Pitch, T.Rotator().Yaw, T.Rotator().Roll)));
    O->SetObjectField(TEXT("scale"), MaskPoint(T.GetScale3D()));
    return O;
}

void MaskWriteReport(FMaskReview& S, bool bPassed, const FString& Status, const FString& Detail)
{
    TSharedRef<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("passed"), bPassed); R->SetStringField(TEXT("status"), Status);
    R->SetStringField(TEXT("detail"), Detail);
    R->SetStringField(TEXT("scope"), TEXT("Isolated standalone Chapter01_House render and native pickup/equipment operations. Assisted camera and player placement; not multiplayer replication or traversal coverage."));
    R->SetStringField(TEXT("userDirectory"), FPaths::ProjectUserDir());
    R->SetNumberField(TEXT("elapsedSeconds"), FPlatformTime::Seconds() - S.Started);
    R->SetNumberField(TEXT("stage"), S.Stage);
    R->SetBoolField(TEXT("collectionAndDamagePassed"), S.bCollected);
    R->SetBoolField(TEXT("removalPassed"), S.bRemovalPassed);
    R->SetBoolField(TEXT("reequipPassed"), S.bReequipPassed);
    R->SetBoolField(TEXT("placementsPassed"), S.bPlacementsPassed);
    R->SetArrayField(TEXT("placements"), S.PlacementResults);
    R->SetArrayField(TEXT("wearing"), S.WearingResults);
    R->SetArrayField(TEXT("captures"), S.Captures);
    FString Text;
    FJsonSerializer::Serialize(R, TJsonWriterFactory<>::Create(&Text));
    IFileManager::Get().MakeDirectory(*S.Directory, true);
    FFileHelper::SaveStringToFile(Text, *(S.Directory / TEXT("review.json")));
}

void MaskFinish(FMaskReview& S, bool bPassed, const FString& Detail)
{
    S.bFinished = true;
    MaskWriteReport(S, bPassed, bPassed ? TEXT("PASS") : TEXT("FAIL"), Detail);
    UE_LOG(LogTemp, Display, TEXT("HOUSE_MASK_REVIEW_RESULT passed=%d stage=%d captures=%d detail=%s report=%s"),
        bPassed ? 1 : 0, S.Stage, S.Captures.Num(), *Detail, *(S.Directory / TEXT("review.json")));
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

void MaskWarmup(FMaskReview& S)
{
#if WITH_EDITOR
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
#endif
    S.Changed = FPlatformTime::Seconds(); S.ChangedFrame = GFrameCounter;
    S.Pending.Reset(); S.Requested = 0;
}

bool MaskWarm(FMaskReview& S)
{
#if WITH_EDITOR
    if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
    { S.Changed = FPlatformTime::Seconds(); S.ChangedFrame = GFrameCounter; return false; }
#endif
    return FPlatformTime::Seconds() - S.Changed >= MaskWarmupSeconds && GFrameCounter - S.ChangedFrame >= MaskWarmupFrames;
}

// Returns true only after a screenshot file exists at the requested resolution.
bool MaskCapture(FMaskReview& S, APlayerController* PC)
{
    if (!MaskWarm(S)) return false;
    const double Now = FPlatformTime::Seconds();
    if (S.Pending.IsEmpty())
    {
        int32 Width = 0, Height = 0; PC->GetViewportSize(Width, Height);
        if (Width != 1280 || (Height != 720 && Height != 960))
        { MaskFinish(S, false, FString::Printf(TEXT("Expected a 1280x720 or 1280x960 rendered viewport; got %dx%d"), Width, Height)); return false; }
        S.Pending = S.Directory / (S.ShotName + TEXT(".png"));
        IFileManager::Get().Delete(*S.Pending, false, true);
        FScreenshotRequest::RequestScreenshot(S.Pending, true, false);
        S.Requested = Now;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_MASK_REVIEW_CAPTURE shot=%s path=%s"), *S.ShotName, *S.Pending);
        return false;
    }
    if (Now - S.Requested < .5) return false;
    const int64 Bytes = IFileManager::Get().FileSize(*S.Pending);
    if (Bytes <= 0)
    { if (Now - S.Requested > 20) MaskFinish(S, false, TEXT("Screenshot did not finish")); return false; }
    int32 Width = 0, Height = 0; PC->GetViewportSize(Width, Height);
    TSharedRef<FJsonObject> C = MakeShared<FJsonObject>();
    C->SetStringField(TEXT("shot"), S.ShotName); C->SetStringField(TEXT("path"), S.Pending);
    C->SetNumberField(TEXT("bytes"), double(Bytes)); C->SetNumberField(TEXT("width"), Width); C->SetNumberField(TEXT("height"), Height);
    C->SetNumberField(TEXT("warmupSeconds"), S.Requested - S.Changed);
    C->SetNumberField(TEXT("warmupFrames"), double(GFrameCounter - S.ChangedFrame));
    if (S.Camera.IsValid()) C->SetObjectField(TEXT("reviewCamera"), MaskTransform(S.Camera->GetActorTransform()));
    S.Captures.Add(MakeShared<FJsonValueObject>(C));
    MaskWriteReport(S, false, TEXT("RUNNING"), S.ShotName);
    return true;
}

USkeletalMeshComponent* MaskBody(UWorld* World, AStoryFirstPersonCharacter* Pawn)
{
    for (TActorIterator<ASeniorBraxtonVisual> It(World); It; ++It)
        if (It->GetOwner() == Pawn && It->GetBodyMesh()) return It->GetBodyMesh();
    for (TActorIterator<ASeniorRunnerVisual> It(World); It; ++It)
        if (It->GetOwner() == Pawn && It->GetBodyMesh()) return It->GetBodyMesh();
    for (TActorIterator<ASeniorFixerVisual> It(World); It; ++It)
        if (It->GetOwner() == Pawn && It->GetBodyMesh()) return It->GetBodyMesh();
    return Pawn->GetMesh();
}

void MaskExposeThirdPerson(UWorld* World, AStoryFirstPersonCharacter* Pawn)
{
    // Review-only: preserve hidden proxy/garment choices, expose the actual
    // assembled character to a detached camera, and hide first-person arms.
    for (TActorIterator<AActor> It(World); It; ++It)
        if (*It == Pawn || It->IsOwnedBy(Pawn))
        {
            TArray<UPrimitiveComponent*> Parts; It->GetComponents(Parts);
            for (UPrimitiveComponent* Part : Parts)
                if (Part && Part != Pawn->FirstPersonArms) Part->SetOwnerNoSee(false);
        }
    if (Pawn->FirstPersonArms) Pawn->FirstPersonArms->SetVisibility(false);
}

void MaskIgnorePlayer(UWorld* World, AStoryFirstPersonCharacter* Pawn, FCollisionQueryParams& Q)
{
    for (TActorIterator<AActor> It(World); It; ++It)
        if (*It == Pawn || It->IsOwnedBy(Pawn) || It->IsA<APawn>() || It->IsA<AHouseOxbloodMaskPickup>()) Q.AddIgnoredActor(*It);
}

bool MaskAimCamera(FMaskReview& S, UWorld* World, APlayerController* PC, const FVector& Target,
    const FVector& PreferredDirection, float Distance, float Lift, float Fov)
{
    FCollisionQueryParams Q(SCENE_QUERY_STAT(HouseMaskReviewCamera), true);
    MaskIgnorePlayer(World, S.Pawn.Get(), Q);
    for (float Angle : { 0.f, 20.f, -20.f, 40.f, -40.f, 90.f, -90.f, 180.f })
    {
        const FVector Direction = FRotator(0, Angle, 0).RotateVector(PreferredDirection).GetSafeNormal();
        const FVector Position = Target + Direction * Distance + FVector(0, 0, Lift);
        FHitResult Hit;
        if (World->LineTraceSingleByChannel(Hit, Target + FVector(0,0,2), Position, ECC_Visibility, Q)) continue;
        S.Camera->SetActorLocationAndRotation(Position, (Target - Position).Rotation());
        S.Camera->GetCameraComponent()->SetFieldOfView(Fov);
        S.Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
        PC->bAutoManageActiveCameraTarget = false;
        PC->SetViewTarget(S.Camera.Get());
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
        MaskWarmup(S);
        return true;
    }
    return false;
}

bool MaskPlacement(FMaskReview& S, UWorld* World, AHouseOxbloodMaskPickup* Pickup, FString& Failure)
{
    UStaticMeshComponent* Display = Pickup->FindComponentByClass<UStaticMeshComponent>();
    if (!Display || !Display->GetStaticMesh()) { Failure = TEXT("Pickup has no display mesh"); return false; }
    TArray<UTextRenderComponent*> Labels; Pickup->GetComponents(Labels);
    for (const UTextRenderComponent* Label : Labels)
        if (Label && Label->IsVisible() && !Label->bHiddenInGame && !Label->Text.IsEmpty())
        { Failure = TEXT("A visible floating pickup title remains"); return false; }

    FVector Lowest = Display->Bounds.Origin - FVector(0,0,Display->Bounds.BoxExtent.Z);
    bool bVertexMeasured = false;
    const UStaticMesh* Mesh = Display->GetStaticMesh();
    if (Mesh->GetRenderData() && !Mesh->GetRenderData()->LODResources.IsEmpty())
    {
        const FPositionVertexBuffer& V = Mesh->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
        if (V.GetNumVertices() > 0)
        {
            Lowest.Z = BIG_NUMBER;
            for (uint32 I = 0; I < V.GetNumVertices(); ++I)
            {
                const FVector P = Display->GetComponentTransform().TransformPosition(FVector(V.VertexPosition(I)));
                if (P.Z < Lowest.Z) Lowest = P;
            }
            bVertexMeasured = true;
        }
    }
    FCollisionQueryParams Q(SCENE_QUERY_STAT(HouseMaskReviewSupport), true, Pickup);
    MaskIgnorePlayer(World, S.Pawn.Get(), Q);
    FHitResult Support;
    const bool bHit = World->LineTraceSingleByChannel(Support, Lowest + FVector(0,0,12), Lowest - FVector(0,0,110), ECC_Visibility, Q);
    const double Gap = bHit ? Lowest.Z - Support.ImpactPoint.Z : 9999;
    TSharedRef<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetStringField(TEXT("pickup"), Pickup->GetName());
    R->SetObjectField(TEXT("actorTransform"), MaskTransform(Pickup->GetActorTransform()));
    R->SetObjectField(TEXT("meshTransform"), MaskTransform(Display->GetComponentTransform()));
    R->SetObjectField(TEXT("lowestPoint"), MaskPoint(Lowest));
    R->SetBoolField(TEXT("vertexMeasured"), bVertexMeasured); R->SetBoolField(TEXT("supportHit"), bHit);
    R->SetStringField(TEXT("supportActor"), GetNameSafe(Support.GetActor()));
    R->SetStringField(TEXT("supportComponent"), GetNameSafe(Support.GetComponent()));
    if (const UStaticMeshComponent* SupportMesh = Cast<UStaticMeshComponent>(Support.GetComponent()))
        R->SetStringField(TEXT("supportMesh"), GetNameSafe(SupportMesh->GetStaticMesh()));
    R->SetObjectField(TEXT("supportPoint"), MaskPoint(Support.ImpactPoint));
    R->SetObjectField(TEXT("supportNormal"), MaskPoint(Support.ImpactNormal));
    R->SetNumberField(TEXT("supportGapCm"), Gap);
    R->SetBoolField(TEXT("floatingLabel"), false);
    S.PlacementResults.Add(MakeShared<FJsonValueObject>(R));
    UE_LOG(LogTemp, Display, TEXT("HOUSE_MASK_REVIEW_SUPPORT pickup=%s actor=%s component=%s lowest=%s support=%s gap=%.3f vertexMeasured=%d"),
        *Pickup->GetName(), *GetNameSafe(Support.GetActor()), *GetNameSafe(Support.GetComponent()),
        *Lowest.ToString(), *Support.ImpactPoint.ToString(), Gap, bVertexMeasured ? 1 : 0);
    if (!bHit || Support.ImpactNormal.Z < .65f) { Failure = TEXT("Pickup has no level supporting surface below its lowest point"); return false; }
    // Existing furniture collision proxies extend up to 2.2 cm above the
    // visible tabletop. Images still show the authored visible contact.
    if (bVertexMeasured && (Gap < -2.8f || Gap > 4.f)) { Failure = TEXT("Pickup mesh is floating above or buried in its support"); return false; }
    return true;
}

void MaskRecordWear(FMaskReview& S, USkeletalMeshComponent* Body)
{
    AStoryFirstPersonCharacter* Pawn = S.Pawn.Get();
    TSharedRef<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetNumberField(TEXT("characterIndex"), S.CharacterIndex); R->SetStringField(TEXT("view"), S.Side == 0 ? TEXT("front") : TEXT("side"));
    R->SetStringField(TEXT("bodyComponent"), GetNameSafe(Body));
    R->SetStringField(TEXT("bodyMesh"), GetNameSafe(Body->GetSkeletalMeshAsset()));
    R->SetObjectField(TEXT("bodyTransform"), MaskTransform(Body->GetComponentTransform()));
    R->SetObjectField(TEXT("headSocketTransform"), MaskTransform(Body->GetSocketTransform(TEXT("head"))));
    R->SetObjectField(TEXT("maskTransform"), MaskTransform(Pawn->HouseMaskVisual->GetComponentTransform()));
    R->SetStringField(TEXT("maskAttachParent"), GetNameSafe(Pawn->HouseMaskVisual->GetAttachParent()));
    R->SetStringField(TEXT("maskAttachSocket"), Pawn->HouseMaskVisual->GetAttachSocketName().ToString());
    R->SetNumberField(TEXT("damageMultiplier"), Pawn->OutgoingDamageMultiplier());
    S.WearingResults.Add(MakeShared<FJsonValueObject>(R));
    UE_LOG(LogTemp, Display, TEXT("HOUSE_MASK_REVIEW_WEAR character=%d view=%d head=%s mask=%s parent=%s socket=%s"),
        S.CharacterIndex, S.Side, *Body->GetSocketTransform(TEXT("head")).ToString(),
        *Pawn->HouseMaskVisual->GetComponentTransform().ToString(), *GetNameSafe(Pawn->HouseMaskVisual->GetAttachParent()),
        *Pawn->HouseMaskVisual->GetAttachSocketName().ToString());
}
}

void TickHouseMaskReview(UWorld* World)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("HouseMaskReview"))) return;
    static FMaskReview S;
    if (!World || S.bFinished) return;
    const double Now = FPlatformTime::Seconds();
    if (S.World.Get() != World)
    {
        S = FMaskReview(); S.World = World; S.Started = Now;
        S.Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("HouseMaskReview"));
        IFileManager::Get().MakeDirectory(*S.Directory, true);
    }
    if (Now - S.Started > 900) { MaskFinish(S, false, TEXT("Bounded mask review timed out")); return; }
    FString UserDir;
    if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || UserDir.IsEmpty())
    { MaskFinish(S, false, TEXT("An explicit isolated -UserDir is required")); return; }
    if (UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House"))
    { MaskFinish(S, false, TEXT("Review requires the actual Chapter01_House map")); return; }
    APlayerController* PC = World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn = PC ? Cast<AStoryFirstPersonCharacter>(PC->GetPawn()) : nullptr;
    if (!PC || !Pawn || !World->GetGameViewport() || !Pawn->HasAuthority()) return;

    if (S.Stage == 0)
    {
        if (Now - S.Started < 3) return;
        S.Pawn = Pawn; S.OriginalLocation = Pawn->GetActorLocation(); S.OriginalRotation = Pawn->GetActorRotation();
        Scalability::FQualityLevels Quality; Quality.SetFromSingleQualityLevel(1); Scalability::SetQualityLevels(Quality);
        if (GEngine)
        {
            GEngine->Exec(World, TEXT("r.SetRes 1280x720w"));
            GEngine->Exec(World, TEXT("r.ScreenPercentage 100"));
            GEngine->Exec(World, TEXT("t.MaxFPS 60"));
            GEngine->Exec(World, TEXT("DisableAllScreenMessages"));
        }
        FActorSpawnParameters P; P.ObjectFlags = RF_Transient;
        P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        S.Camera = World->SpawnActor<ACameraActor>(FVector::ZeroVector, FRotator::ZeroRotator, P);
        if (!S.Camera.IsValid()) { MaskFinish(S, false, TEXT("Detached review camera failed to spawn")); return; }
        for (TActorIterator<AHouseOxbloodMaskPickup> It(World); It; ++It) S.Pickups.Add(*It);
        S.Pickups.Sort([](const TWeakObjectPtr<AHouseOxbloodMaskPickup>& A, const TWeakObjectPtr<AHouseOxbloodMaskPickup>& B)
            { return A->GetName() < B->GetName(); });
        int32 Expected = 4; FParse::Value(FCommandLine::Get(), TEXT("HouseMaskExpectedPickups="), Expected);
        if (S.Pickups.Num() != Expected || S.Pickups.IsEmpty())
        { MaskFinish(S, false, FString::Printf(TEXT("Expected %d pickups; found %d"), Expected, S.Pickups.Num())); return; }
        if (Pawn->bHasHouseMask) { MaskFinish(S, false, TEXT("Isolated player unexpectedly already owns a mask")); return; }
        Pawn->GetCharacterMovement()->StopMovementImmediately(); Pawn->GetCharacterMovement()->DisableMovement();
        PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
        MaskWarmup(S); S.Stage = 1; return;
    }
    if (S.Stage == 1)
    {
        if (!MaskWarm(S)) return;
        if (S.PickupIndex >= S.Pickups.Num()) { S.Stage = 3; return; }
        AHouseOxbloodMaskPickup* Pickup = S.Pickups[S.PickupIndex].Get();
        if (!Pickup) { MaskFinish(S, false, TEXT("A placed pickup disappeared before collection")); return; }
        FString Failure;
        if (!MaskPlacement(S, World, Pickup, Failure))
        {
            S.bPlacementsPassed = false;
            UE_LOG(LogTemp, Warning, TEXT("HOUSE_MASK_REVIEW_PLACEMENT_FAILED %s"), *Failure);
            for (TActorIterator<AActor> It(World); It; ++It)
                for (UStaticMeshComponent* Mesh : TInlineComponentArray<UStaticMeshComponent*>(*It))
                    if (Mesh->GetStaticMesh() && FVector::Dist2D(Mesh->Bounds.Origin, Pickup->GetActorLocation()) < 220)
                        UE_LOG(LogTemp, Display, TEXT("HOUSE_MASK_NEARBY actor=%s mesh=%s at=%s rot=%s bounds=%s extents=%s"),
                            *It->GetName(), *GetNameSafe(Mesh->GetStaticMesh()), *Mesh->GetComponentLocation().ToString(),
                            *Mesh->GetComponentRotation().ToString(), *Mesh->Bounds.Origin.ToString(), *Mesh->Bounds.BoxExtent.ToString());
        }
        S.ShotName = FString::Printf(TEXT("pickup_%02d_furniture"), S.PickupIndex + 1);
        if (!MaskAimCamera(S, World, PC, Pickup->InteractionLocation(), FVector(1,-1,0), 115, 85, 54))
        { MaskFinish(S, false, TEXT("Could not find an unobstructed furniture pickup camera")); return; }
        S.Stage = 2; return;
    }
    if (S.Stage == 2)
    {
        if (MaskCapture(S, PC)) { ++S.PickupIndex; S.Stage = 1; }
        return;
    }
    if (S.Stage == 3)
    {
        AHouseOxbloodMaskPickup* Pickup = S.Pickups[0].Get();
        if (!Pickup) { MaskFinish(S, false, TEXT("Collection target is missing")); return; }
        Pawn->SetActorLocation(Pickup->GetActorLocation() + FVector(115,0,80), false, nullptr, ETeleportType::TeleportPhysics);
        if (!Pickup->CanCollect(Pawn) || !Pickup->Collect(Pawn) || !Pawn->bHasHouseMask || !Pawn->bHouseMaskEquipped
            || !FMath::IsNearlyEqual(Pawn->OutgoingDamageMultiplier(), 1.25f, .001f))
        { MaskFinish(S, false, TEXT("Native pickup/equip/+25 percent damage check failed")); return; }
        S.bCollected = true;
        // Open part of the actual great room; only the observer is repositioned.
        Pawn->SetActorLocationAndRotation(FVector(1116,691,110), FRotator(0,0,0), false, nullptr, ETeleportType::TeleportPhysics);
        PC->SetControlRotation(FRotator::ZeroRotator);
        S.Stage = 4; return;
    }
    if (S.Stage == 4)
    {
        ASeniorLobbyPlayerState* Player = Pawn->GetPlayerState<ASeniorLobbyPlayerState>();
        if (!Player) { MaskFinish(S, false, TEXT("Native roster PlayerState is missing")); return; }
        Player->CharacterIndex = S.CharacterIndex; Player->OnRep_CharacterIndex(); Pawn->ApplySelectedCharacter();
        MaskExposeThirdPerson(World, Pawn);
        if (AHUD* HUD = PC->GetHUD()) HUD->bShowHUD = false;
        MaskWarmup(S); S.Stage = 5; return;
    }
    if (S.Stage == 5)
    {
        if (!MaskWarm(S)) return;
        USkeletalMeshComponent* Body = MaskBody(World, Pawn);
        if (!Body || !Body->DoesSocketExist(TEXT("head")) || !Pawn->HouseMaskVisual || !Pawn->HouseMaskVisual->IsVisible())
        { MaskFinish(S, false, TEXT("Native head or visible equipped mask is missing")); return; }
        MaskExposeThirdPerson(World, Pawn);
        const FVector Head = Body->GetSocketLocation(TEXT("head"));
        const FVector Forward = Pawn->GetActorForwardVector();
        const FVector Right = FVector::CrossProduct(Forward, FVector::UpVector).GetSafeNormal();
        const FVector Direction = S.Side == 0 ? Forward : (Forward*.15f + Right*.99f).GetSafeNormal();
        S.ShotName = FString::Printf(TEXT("character_%d_%s"), S.CharacterIndex, S.Side == 0 ? TEXT("front") : TEXT("side"));
        MaskRecordWear(S, Body);
        if (!MaskAimCamera(S, World, PC, Head + FVector(0,0,6), Direction, 105, 4, 42))
        { MaskFinish(S, false, TEXT("Could not find an unobstructed equipped character camera")); return; }
        S.Stage = 6; return;
    }
    if (S.Stage == 6)
    {
        if (!MaskCapture(S, PC)) return;
        if (S.Side == 0) { S.Side = 1; MaskWarmup(S); S.Stage = 5; return; }
        if (++S.CharacterIndex < 4) { S.Side = 0; S.Stage = 4; return; }
        S.Stage = 7; return;
    }
    if (S.Stage == 7)
    {
        Pawn->ToggleHouseMask();
        S.bRemovalPassed = Pawn->bHasHouseMask && !Pawn->bHouseMaskEquipped
            && FMath::IsNearlyEqual(Pawn->OutgoingDamageMultiplier(), 1.f, .001f)
            && Pawn->HouseMaskVisual && !Pawn->HouseMaskVisual->IsVisible();
        if (!S.bRemovalPassed) { MaskFinish(S, false, TEXT("Native mask removal did not hide the mesh and clear damage bonus")); return; }
        S.ShotName = TEXT("character_3_removed"); MaskWarmup(S); S.Stage = 8; return;
    }
    if (S.Stage == 8)
    { if (MaskCapture(S, PC)) S.Stage = 9; return; }
    if (S.Stage == 9)
    {
        Pawn->ToggleHouseMask();
        S.bReequipPassed = Pawn->bHasHouseMask && Pawn->bHouseMaskEquipped
            && FMath::IsNearlyEqual(Pawn->OutgoingDamageMultiplier(), 1.25f, .001f)
            && Pawn->HouseMaskVisual && Pawn->HouseMaskVisual->IsVisible();
        if (!S.bReequipPassed) { MaskFinish(S, false, TEXT("Native mask re-equip did not restore mesh and damage bonus")); return; }
        S.ShotName = TEXT("character_3_reequipped"); MaskWarmup(S); S.Stage = 10; return;
    }
    if (S.Stage == 10)
    {
        if (!MaskCapture(S, PC)) return;
        MaskFinish(S, S.bPlacementsPassed, S.bPlacementsPassed ? TEXT("All placed pickups supported and label-free; native collection, +25 percent damage, removal and re-equip passed. Front and side captures produced for three native characters and one fallback; inspect the images to judge fit.") : TEXT("Equipment operations passed; placement support failures recorded for visual diagnosis."));
    }
}
#endif
