#include "HouseFixtureSmoke.h"

#if !UE_BUILD_SHIPPING
#include "HouseDamageSubsystem.h"
#include "HouseInteractiveProp.h"
#include "HouseReactiveFixture.h"
#include "ExistingHouseDoor.h"
#include "StoryCampaign.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
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
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
struct FFixtureSmokeState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AHouseReactiveFixture> Curtain, Window, Lamp;
    TWeakObjectPtr<AExistingHouseDoor> PorchDoor;
    TWeakObjectPtr<AHouseReactiveFixture> PorchGlass;
    TArray<TWeakObjectPtr<AActor>> Created;
    FVector CurtainAnchor = FVector::ZeroVector, LampAnchor = FVector::ZeroVector;
    FVector PorchPoint = FVector::ZeroVector, PorchNormal = FVector::ZeroVector;
    FTransform SavedPawnTransform;
    FRotator SavedView = FRotator::ZeroRotator;
    TSharedPtr<FJsonObject> Report;
    double Started = 0, StageStarted = 0;
    double LastPorchShot = 0;
    int32 Stage = 0, CountBefore = 0;
    int32 PorchShots = 0;
    float MaxSwing = 0;
    bool bPorchFirstCrack = false, bPorchSecondCrack = false;
    bool bPorchSettledCapture = false;
    bool bPorchFloorAimed = false;
    FString ClearanceFailure;
    bool bFinished = false;
};

void FixtureCleanup(FFixtureSmokeState& State, AStoryFirstPersonCharacter* Pawn, APlayerController* PC)
{
    for (const TWeakObjectPtr<AActor>& Actor : State.Created)
        if (Actor.IsValid()) Actor->Destroy();
    if (Pawn)
    {
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->SetActorTransform(State.SavedPawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
    }
    if (PC) PC->SetControlRotation(State.SavedView);
    State.bFinished = true;
}

AStaticMeshActor* MakeFixtureTestCube(UWorld* World, FFixtureSmokeState& State,
    UStaticMesh* Cube, const FVector& Position, const FVector& Scale, bool bPhysics)
{
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
        FTransform(Position), Params);
    if (!Actor) return nullptr;
    State.Created.Add(Actor);
    UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(Cube);
    Mesh->SetWorldScale3D(Scale);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    if (bPhysics)
    {
        Mesh->SetCollisionObjectType(ECC_PhysicsBody);
        Mesh->SetNotifyRigidBodyCollision(true);
        Mesh->BodyInstance.bUseCCD = true;
        Mesh->SetSimulatePhysics(true);
        Mesh->SetMassOverrideInKg(NAME_None, 8.f);
    }
    return Actor;
}

AHouseReactiveFixture* MakeFixture(UWorld* World, FFixtureSmokeState& State, UStaticMesh* Cube,
    EHouseReactiveFixtureKind Kind, const FVector& Position, const FVector& Scale, const FVector& Pivot)
{
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AHouseReactiveFixture* Fixture = World->SpawnActor<AHouseReactiveFixture>(
        AHouseReactiveFixture::StaticClass(), FTransform(Position), Params);
    if (!Fixture) return nullptr;
    State.Created.Add(Fixture);
    Fixture->FixtureKind = Kind;
    Fixture->FixtureId = FName(*FString::Printf(TEXT("SmokeFixture_%d"), int32(Kind)));
    Fixture->OriginalMesh = Cube;
    Fixture->MeshTransform = FTransform(FQuat::Identity, FVector::ZeroVector, Scale);
    Fixture->MotionPivotLocal = Pivot;
    Fixture->CurtainAxis = 1;
    Fixture->RefreshFixture();
    return Fixture;
}

void CapturePorch(FFixtureSmokeState& State, const TCHAR* Name, const TCHAR* ReportField)
{
    FString Evidence;
    if (FParse::Value(FCommandLine::Get(),TEXT("HouseInteractivityEvidence="),Evidence)
        && !Evidence.IsEmpty() && !FScreenshotRequest::IsScreenshotRequested())
    {
        const FString File=FPaths::Combine(Evidence,Name);
        FScreenshotRequest::RequestScreenshot(File,false,false);
        State.Report->SetStringField(ReportField,File);
    }
}

bool CheckFixtureMotionContinuity(UWorld* World, FFixtureSmokeState& State, UStaticMesh* Cube,
    AStoryFirstPersonCharacter* Pawn, FString& Failure)
{
    // Exercise the production Tick with equal elapsed time at different frame
    // rates. These transient meshes are removed before the physical-hit test.
    AHouseReactiveFixture* Cloth30=MakeFixture(World,State,Cube,EHouseReactiveFixtureKind::Curtain,
        FVector(80,3000,8120),FVector(.035f,1.8f,2.f),FVector(0,-90,100));
    AHouseReactiveFixture* Cloth120=MakeFixture(World,State,Cube,EHouseReactiveFixtureKind::Curtain,
        FVector(80,3000,8120),FVector(.035f,1.8f,2.f),FVector(0,-90,100));
    AHouseReactiveFixture* Lamp30=MakeFixture(World,State,Cube,EHouseReactiveFixtureKind::Chandelier,
        FVector(600,3000,9000),FVector(.6f,.6f,.18f),FVector(0,0,100));
    AHouseReactiveFixture* Lamp120=MakeFixture(World,State,Cube,EHouseReactiveFixtureKind::Chandelier,
        FVector(600,3000,9000),FVector(.6f,.6f,.18f),FVector(0,0,100));
    AHouseReactiveFixture* LampHitch=MakeFixture(World,State,Cube,EHouseReactiveFixtureKind::Chandelier,
        FVector(600,3000,9000),FVector(.6f,.6f,.18f),FVector(0,0,100));
    if (!Cloth30 || !Cloth120 || !Lamp30 || !Lamp120 || !LampHitch)
    { Failure=TEXT("Could not create fixture motion checks.");return false; }
    if (!Cloth30->Interact(Pawn) || !Cloth120->Interact(Pawn))
    { Failure=TEXT("Fixture motion check could not begin curtain movement.");return false; }
    for (int32 Step=0;Step<6;++Step) Cloth30->Tick(1.f/30.f);
    for (int32 Step=0;Step<24;++Step) Cloth120->Tick(1.f/120.f);
    float ClothFrameDelta=FMath::Abs(Cloth30->GetCurtainProgress()-Cloth120->GetCurtainProgress());
    const float BeforeReverse=Cloth30->GetCurtainVelocity();
    Cloth30->Interact(Pawn);Cloth120->Interact(Pawn);
    const bool bContinuous=BeforeReverse>0.f
        && FMath::IsNearlyEqual(BeforeReverse,Cloth30->GetCurtainVelocity(),.0001f);
    for (int32 Step=0;Step<6;++Step) Cloth30->Tick(1.f/30.f);
    for (int32 Step=0;Step<24;++Step) Cloth120->Tick(1.f/120.f);
    ClothFrameDelta=FMath::Max(ClothFrameDelta,
        FMath::Abs(Cloth30->GetCurtainProgress()-Cloth120->GetCurtainProgress()));
    for (int32 Step=0;Step<42;++Step) Cloth30->Tick(1.f/30.f);
    for (int32 Step=0;Step<168;++Step) Cloth120->Tick(1.f/120.f);
    for (AHouseReactiveFixture* Lamp:{Lamp30,Lamp120,LampHitch})
        Lamp->ApplyImpact(Lamp->GetActorLocation(),FVector(1,.3f,0),30.f);
    for (int32 Step=0;Step<30;++Step) Lamp30->Tick(1.f/30.f);
    for (int32 Step=0;Step<120;++Step) Lamp120->Tick(1.f/120.f);
    LampHitch->Tick(1.f);
    const float LampFrameDelta=FVector2D::Distance(Lamp30->GetSwingAngles(),Lamp120->GetSwingAngles());
    const float HitchDelta=FVector2D::Distance(LampHitch->GetSwingAngles(),Lamp120->GetSwingAngles());
    State.Report->SetBoolField(TEXT("curtain_reversal_keeps_velocity"),bContinuous);
    State.Report->SetNumberField(TEXT("curtain_30_120_fps_progress_delta"),ClothFrameDelta);
    State.Report->SetNumberField(TEXT("lamp_30_120_fps_angle_delta"),LampFrameDelta);
    State.Report->SetNumberField(TEXT("lamp_one_second_hitch_angle_delta"),HitchDelta);
    const bool bPassed=bContinuous && ClothFrameDelta<.0001f && LampFrameDelta<.001f && HitchDelta<.001f
        && Cloth30->GetCurtainProgress()==0.f && Cloth120->GetCurtainProgress()==0.f;
    for (AHouseReactiveFixture* Fixture:{Cloth30,Cloth120,Lamp30,Lamp120,LampHitch}) Fixture->Destroy();
    if (!bPassed) Failure=TEXT("Fixture motion changed across frame rates or snapped during curtain reversal.");
    return bPassed;
}
}

bool TickHouseFixtureSmoke(UWorld* World, AStoryFirstPersonCharacter* Pawn, APlayerController* PC,
    FString& OutFailure, TSharedPtr<FJsonObject>& OutReport)
{
    static FFixtureSmokeState State;
    if (State.World.Get() != World)
    {
        State = FFixtureSmokeState();
        State.World = World;
    }
    if (State.bFinished) { OutReport = State.Report; return true; }
    if (!World || !Pawn || !PC)
    {
        OutFailure = TEXT("Fixture smoke needs the native character and controller.");
        return true;
    }
    const double Now = World->GetTimeSeconds();
    const auto Fail = [&](const FString& Message)
    {
        OutFailure = Message;
        OutReport = State.Report;
        FixtureCleanup(State, Pawn, PC);
        return true;
    };
    if (State.Stage && Now - State.Started > 65.f)
        return Fail(FString::Printf(TEXT("Fixture smoke timed out at stage %d."), State.Stage));

    if (State.Stage == 0)
    {
        State.Report = MakeShared<FJsonObject>();
        State.Started = State.StageStarted = Now;
        State.SavedPawnTransform = Pawn->GetActorTransform();
        State.SavedView = PC->GetControlRotation();
        UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (!Cube) return Fail(TEXT("Fixture smoke could not load its transient test mesh."));
        MakeFixtureTestCube(World, State, Cube, FVector(0, 3000, 7990), FVector(20,20,.2f), false);
        State.Curtain = MakeFixture(World, State, Cube, EHouseReactiveFixtureKind::Curtain,
            FVector(140,3000,8120), FVector(.035f,1.8f,2.f), FVector(0,-90,100));
        State.Window = MakeFixture(World, State, Cube, EHouseReactiveFixtureKind::Window,
            FVector(280,3270,8120), FVector(.03f,1.5f,1.8f), FVector::ZeroVector);
        State.Lamp = MakeFixture(World, State, Cube, EHouseReactiveFixtureKind::Chandelier,
            FVector(200,2800,8275), FVector(.6f,.6f,.18f), FVector(0,0,100));
        if (!State.Curtain.IsValid() || !State.Window.IsValid() || !State.Lamp.IsValid())
            return Fail(TEXT("Could not create transient fixtures."));
        State.CurtainAnchor = State.Curtain->GetFixtureMesh()->GetAttachParent()->GetComponentLocation();
        State.LampAnchor = State.Lamp->GetFixtureMesh()->GetAttachParent()->GetComponentLocation();
        const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->SetActorLocation(FVector(0,3000,8000 + Half + 1), false, nullptr, ETeleportType::TeleportPhysics);
        PC->SetControlRotation(FRotator(0,0,0));
        FString MotionFailure;
        if (!CheckFixtureMotionContinuity(World,State,Cube,Pawn,MotionFailure)) return Fail(MotionFailure);
        State.Stage = 1;
        return false;
    }
    if (State.Stage == 1 && Now - State.StageStarted > .25f)
    {
        if (!State.Curtain->Interact(Pawn)) return Fail(TEXT("Native curtain interaction refused a reachable player."));
        State.Stage = 2; State.StageStarted = Now;
    }
    else if (State.Stage == 2 && Now - State.StageStarted > 1.35f)
    {
        State.Report->SetNumberField(TEXT("curtain_open_progress"), State.Curtain->GetCurtainProgress());
        if (!State.Curtain->IsOpen() || State.Curtain->GetCurtainProgress() < .999f)
            return Fail(TEXT("Curtain did not complete opening."));
        const float AnchorDelta = FVector::Dist(State.CurtainAnchor,
            State.Curtain->GetFixtureMesh()->GetAttachParent()->GetComponentLocation());
        State.Report->SetNumberField(TEXT("curtain_anchor_delta_cm"), AnchorDelta);
        if (AnchorDelta > .01f) return Fail(TEXT("Curtain moved its fixed anchor."));
        if (State.Curtain->GetFixtureCollision()->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Ignore)
            return Fail(TEXT("Curtain blocks the character."));
        if (!State.Curtain->Interact(Pawn)) return Fail(TEXT("Native curtain interaction could not close it."));
        State.Stage = 3; State.StageStarted = Now;
    }
    else if (State.Stage == 3 && Now - State.StageStarted > 1.35f)
    {
        State.Report->SetNumberField(TEXT("curtain_closed_progress"), State.Curtain->GetCurtainProgress());
        if (State.Curtain->IsOpen() || State.Curtain->GetCurtainProgress() > .001f)
            return Fail(TEXT("Curtain did not complete closing."));
        UHouseDamageSubsystem* Damage = World->GetSubsystem<UHouseDamageSubsystem>();
        if (!Damage) return Fail(TEXT("House damage subsystem is missing."));
        State.CountBefore = Damage->GetTotalBrokenCount();
        FHitResult Hit;
        const FVector Center = State.Window->GetFixtureMesh()->Bounds.Origin;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(FixturePaneCheck), false, Pawn);
        if (!World->LineTraceSingleByChannel(Hit, Center - FVector(150,0,0), Center + FVector(150,0,0), ECC_Visibility, Query)
            || Hit.GetActor() != State.Window.Get()) return Fail(TEXT("Intact window does not block the weapon trace."));
        UGameplayStatics::ApplyPointDamage(State.Window.Get(), 8.f, FVector::ForwardVector, Hit,
            PC, Pawn, UDamageType::StaticClass());
        const int32 FirstCracks = State.Window->GetCrackCount();
        if (State.Window->IsBroken() || State.Window->GetGlassDamage()!=8.f || FirstCracks<=0
            || State.Window->GetFixtureCollision()->GetCollisionEnabled()==ECollisionEnabled::NoCollision
            || Damage->GetTotalBrokenCount()!=State.CountBefore)
            return Fail(TEXT("First glass strike should crack an intact, still-solid pane without counting a break."));
        UGameplayStatics::ApplyPointDamage(State.Window.Get(), 8.f, FVector::ForwardVector, Hit,
            PC, Pawn, UDamageType::StaticClass());
        if (State.Window->IsBroken() || State.Window->GetGlassDamage()!=16.f || State.Window->GetCrackCount()<=FirstCracks)
            return Fail(TEXT("Second glass strike should extend the cracks while retaining the pane."));
        State.Report->SetNumberField(TEXT("glass_first_hit_crack_lines"),FirstCracks);
        State.Report->SetNumberField(TEXT("glass_second_hit_crack_lines"),State.Window->GetCrackCount());
        State.Report->SetNumberField(TEXT("glass_crack_sound_events"),State.Window->GetGlassCrackSoundCount());
        if (State.Window->GetGlassCrackSoundCount()!=2)
            return Fail(TEXT("Cracked glass did not trigger a distinct sound on both strikes."));
        UGameplayStatics::ApplyPointDamage(State.Window.Get(), 8.f, FVector::ForwardVector, Hit,
            PC, Pawn, UDamageType::StaticClass());
        const bool bNoCollision = State.Window->GetFixtureCollision()->GetCollisionEnabled() == ECollisionEnabled::NoCollision;
        State.Report->SetBoolField(TEXT("glass_collision_removed"), bNoCollision);
        State.Report->SetNumberField(TEXT("glass_shard_count"), State.Window->GetShardCount());
        State.Report->SetNumberField(TEXT("glass_shatter_sound_events"),State.Window->GetGlassShatterSoundCount());
        if (!State.Window->IsBroken() || !bNoCollision || State.Window->GetShardCount() != 56 || State.Window->GetCrackCount()!=0)
            return Fail(TEXT("Window did not shatter, remove collision and create its bounded debris."));
        if (State.Window->GetGlassShatterSoundCount()!=1)
            return Fail(TEXT("Shattered glass did not trigger its one-shot break sound."));
        if (World->LineTraceSingleByChannel(Hit, Center - FVector(150,0,0), Center + FVector(150,0,0), ECC_Visibility, Query))
            return Fail(TEXT("Broken window leaves an invisible blocking pane."));
        UGameplayStatics::ApplyPointDamage(State.Window.Get(), 50.f, FVector::ForwardVector, Hit,
            PC, Pawn, UDamageType::StaticClass());
        if (Damage->GetTotalBrokenCount() != State.CountBefore + 1)
            return Fail(TEXT("Window break was not counted exactly once."));
        State.Report->SetNumberField(TEXT("glass_count_delta_after_repeated_hit"), Damage->GetTotalBrokenCount() - State.CountBefore);
        UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        AStaticMeshActor* Projectile = MakeFixtureTestCube(World, State, Cube, FVector(0,2800,8275), FVector(.24f), true);
        if (!Projectile) return Fail(TEXT("Could not spawn the physics object for the hanging light."));
        Projectile->GetStaticMeshComponent()->SetPhysicsLinearVelocity(FVector(1000,0,0));
        State.Stage = 4; State.StageStarted = Now;
    }
    else if (State.Stage == 4)
    {
        State.MaxSwing = FMath::Max(State.MaxSwing, float(State.Lamp->GetSwingAngles().Size()));
        if (Now - State.StageStarted > 2.f && State.Lamp->GetImpactCount() == 0)
            return Fail(TEXT("A real moving physics object failed to rock the hanging light."));
        if (Now - State.StageStarted > 11.f)
        {
            const float FinalSwing = State.Lamp->GetSwingAngles().Size();
            const float AnchorDelta = FVector::Dist(State.LampAnchor,
                State.Lamp->GetFixtureMesh()->GetAttachParent()->GetComponentLocation());
            State.Report->SetNumberField(TEXT("lamp_physics_impacts"), State.Lamp->GetImpactCount());
            State.Report->SetNumberField(TEXT("lamp_peak_swing_degrees"), State.MaxSwing);
            State.Report->SetNumberField(TEXT("lamp_settled_swing_degrees"), FinalSwing);
            State.Report->SetNumberField(TEXT("lamp_anchor_delta_cm"), AnchorDelta);
            if (State.MaxSwing < .5f || FinalSwing > .25f || AnchorDelta > .01f)
                return Fail(TEXT("The hanging light did not rock and settle around its fixed ceiling anchor."));
            if (State.Window->GetShardCount() != 56)
                return Fail(TEXT("Window debris grew during the settling period."));
            State.Report->SetNumberField(TEXT("glass_shards_resting_on_floor"),State.Window->GetRestingShardCount());
            State.Report->SetNumberField(TEXT("glass_shards_supported_by_floor"),State.Window->GetSupportedShardCount());
            if (State.Window->GetRestingShardCount()<40 || State.Window->GetSupportedShardCount()<40)
                return Fail(TEXT("Most window shards did not remain visibly settled on the floor."));
            for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
                if (It->OriginalGlass && It->GetPorchGlassFixture() && !It->IsOpen()
                    && !It->GetPorchGlassFixture()->IsBroken())
                {
                    State.PorchDoor = *It;
                    State.PorchGlass = It->GetPorchGlassFixture();
                    break;
                }
            if (!State.PorchDoor.IsValid()) return Fail(TEXT("No intact native porch glass door was available."));
            UStaticMeshComponent* Pane = State.PorchGlass->GetFixtureMesh();
            const FBox Bounds = State.PorchGlass->OriginalMesh->GetBoundingBox();
            FVector Local = Bounds.GetCenter();
            Local.Z += Bounds.GetExtent().Z * .42f;
            State.PorchPoint = Pane->GetComponentTransform().TransformPosition(Local);
            State.PorchNormal = Pane->GetComponentTransform().TransformVectorNoScale(FVector::YAxisVector).GetSafeNormal();
            const float LowZ = Pane->GetComponentTransform().TransformPosition(Bounds.Min).Z;
            bool bPlaced = false;
            for (float Side : {1.f, -1.f})
            {
                for (float Distance : {180.f, 150.f, 210.f})
                {
                    FVector Candidate = State.PorchPoint + State.PorchNormal * Side * Distance;
                    FHitResult Floor;
                    FCollisionQueryParams FloorQuery(SCENE_QUERY_STAT(PorchGlassTestFloor), false, Pawn);
                    FloorQuery.AddIgnoredActor(State.PorchDoor.Get());
                    FloorQuery.AddIgnoredActor(State.PorchGlass.Get());
                    if (!World->LineTraceSingleByChannel(Floor,
                        FVector(Candidate.X,Candidate.Y,LowZ+55.f), FVector(Candidate.X,Candidate.Y,LowZ-150.f),
                        ECC_Visibility, FloorQuery) || Floor.ImpactNormal.Z < .65f) continue;
                    Candidate.Z = Floor.ImpactPoint.Z + Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f;
                    if (Pawn->TeleportTo(Candidate, (State.PorchPoint-Candidate).Rotation(), false, false))
                    { bPlaced = true; break; }
                }
                if (bPlaced) break;
            }
            if (!bPlaced) return Fail(TEXT("Could not find a clear native porch approach for the window test."));
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            PC->SetControlRotation((State.PorchPoint-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
            State.Report->SetStringField(TEXT("porch_door_id"), State.PorchDoor->DoorId.ToString());
            State.Stage = 5; State.StageStarted = Now;
        }
    }
    else if (State.Stage == 5 && Now - State.StageStarted > .4f)
    {
        PC->SetControlRotation((State.PorchPoint-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        State.CountBefore = World->GetSubsystem<UHouseDamageSubsystem>()->GetTotalBrokenCount();
        Pawn->ThrowEquippedWeapon();
        State.LastPorchShot = Now; State.PorchShots=1;
        State.Stage = 6; State.StageStarted = Now;
    }
    else if (State.Stage == 6)
    {
        if (!State.PorchGlass->IsBroken())
        {
            const float Damage = State.PorchGlass->GetGlassDamage();
            if (Damage >= 8.f && !State.bPorchFirstCrack)
            {
                if (State.PorchGlass->GetCrackCount()<=0
                    || State.PorchDoor->GetPanelCollision()->GetCollisionEnabled()==ECollisionEnabled::NoCollision
                    || World->GetSubsystem<UHouseDamageSubsystem>()->GetTotalBrokenCount()!=State.CountBefore)
                    return Fail(TEXT("The first porch weapon strike did not leave cracked, solid glass."));
                State.bPorchFirstCrack=true;
                CapturePorch(State,TEXT("porch_glass_first_crack.png"),TEXT("porch_cracked_capture"));
            }
            if (Damage >= 16.f) State.bPorchSecondCrack = State.PorchGlass->GetCrackCount()>0;
            if (Now-State.LastPorchShot > 2.5f && State.PorchShots<7)
            {
                PC->SetControlRotation((State.PorchPoint-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
                Pawn->ThrowEquippedWeapon();
                State.LastPorchShot=Now; ++State.PorchShots;
            }
            if (Now-State.StageStarted > 20.f) return Fail(TEXT("Repeated equipped weapon strikes did not shatter native porch glass."));
            return false;
        }
        if (!State.bPorchFirstCrack || !State.bPorchSecondCrack)
            return Fail(TEXT("Native porch weapon strikes skipped one of the intact cracked stages."));
        CapturePorch(State,TEXT("porch_glass_shattered.png"),TEXT("porch_shattered_capture"));
        if (World->GetSubsystem<UHouseDamageSubsystem>()->GetTotalBrokenCount() != State.CountBefore+1)
            return Fail(TEXT("Native porch glass did not count once toward house damage."));
        if (State.PorchDoor->GetPanelCollision()->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
            return Fail(TEXT("Broken native porch glass left its old full-panel blocker enabled."));
        UStaticMeshComponent* Pane = State.PorchGlass->GetFixtureMesh();
        const FBox PaneBounds = State.PorchGlass->OriginalMesh->GetBoundingBox();
        FCollisionQueryParams Query(SCENE_QUERY_STAT(NativePorchGlassGap), false, Pawn);
        bool bGapClear = false;
        // Check an actual glass opening, allowing the original horizontal bars
        // to remain solid at their authored positions.
        for (float X : {-.35f, .35f})
            for (float Z : {-.6f, -.25f, .25f, .6f})
            {
                FVector Local = PaneBounds.GetCenter() + PaneBounds.GetExtent()*FVector(X,0,Z);
                const FVector Point = Pane->GetComponentTransform().TransformPosition(Local);
                FHitResult Hit;
                if (!World->LineTraceSingleByChannel(Hit,Point-State.PorchNormal*22.f,Point+State.PorchNormal*22.f,
                    ECC_Visibility,Query)) bGapClear = true;
            }
        if (!bGapClear) return Fail(TEXT("Actual porch frame collision still fills every broken glass opening."));
        UStaticMeshComponent* Frame = State.PorchDoor->GetOriginalLeafComponent();
        const FBox FrameBounds = State.PorchDoor->OriginalLeaf->GetBoundingBox();
        FVector FrameLocal = FrameBounds.GetCenter();
        FrameLocal.X = FrameBounds.Min.X + 3.f;
        FrameLocal.Y = PaneBounds.GetCenter().Y;
        FrameLocal.Z = PaneBounds.GetCenter().Z;
        const FVector FramePoint = Frame->GetComponentTransform().TransformPosition(FrameLocal);
        FHitResult FrameHit;
        if (!World->LineTraceSingleByChannel(FrameHit,FramePoint-State.PorchNormal*22.f,FramePoint+State.PorchNormal*22.f,
            ECC_Visibility,Query) || FrameHit.GetActor()!=State.PorchDoor.Get())
            return Fail(TEXT("The native porch frame lost collision when its glass broke."));
        State.Report->SetBoolField(TEXT("porch_equipped_weapon_shattered_glass"),true);
        State.Report->SetBoolField(TEXT("porch_first_second_weapon_hits_cracked"),true);
        State.Report->SetNumberField(TEXT("porch_weapon_throws"),State.PorchShots);
        State.Report->SetBoolField(TEXT("porch_pane_clear_frame_solid"),true);
        FVector PassageCenter = Pane->GetComponentTransform().TransformPosition(PaneBounds.GetCenter());
        const float CapsuleRadius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const float CapsuleHalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        // This is a ground-level porch passage. Test a standing player's feet
        // at the threshold rather than suspending their capsule at glass centre,
        // which incorrectly places the head into the retained overhead closer.
        const float ThresholdZ=Frame->GetComponentTransform().TransformPosition(FrameBounds.Min).Z;
        PassageCenter.Z=ThresholdZ+CapsuleHalfHeight+2.f;
        FHitResult CapsuleHit;
        const bool bCapsuleBlocked = World->SweepSingleByChannel(CapsuleHit,
            PassageCenter-State.PorchNormal*95.f,PassageCenter+State.PorchNormal*95.f,FQuat::Identity,
            ECC_Pawn,FCollisionShape::MakeCapsule(CapsuleRadius,CapsuleHalfHeight),Query);
        State.Report->SetBoolField(TEXT("porch_player_capsule_passes_broken_opening"),!bCapsuleBlocked);
        State.Report->SetNumberField(TEXT("tested_capsule_radius_cm"),CapsuleRadius);
        State.Report->SetNumberField(TEXT("tested_capsule_halfheight_cm"),CapsuleHalfHeight);
        State.Report->SetNumberField(TEXT("porch_test_capsule_feet_z"),PassageCenter.Z-CapsuleHalfHeight);
        State.Report->SetStringField(TEXT("porch_broken_frame_mesh"),GetNameSafe(Frame->GetStaticMesh()));
        if (bCapsuleBlocked)
        {
            State.Report->SetStringField(TEXT("porch_capsule_blocker_actor"),GetNameSafe(CapsuleHit.GetActor()));
            State.Report->SetStringField(TEXT("porch_capsule_blocker_component"),GetNameSafe(CapsuleHit.GetComponent()));
            State.ClearanceFailure=FString::Printf(TEXT("Shattered porch opening blocks player capsule: %s / %s."),
                *GetNameSafe(CapsuleHit.GetActor()),*GetNameSafe(CapsuleHit.GetComponent()));
        }
        if (!State.PorchDoor->Interact(Pawn)) return Fail(TEXT("Broken porch door could not still open normally."));
        State.Stage = 7; State.StageStarted = Now;
    }
    else if (State.Stage == 7 && Now-State.StageStarted > .8f)
    {
        if (!State.PorchDoor->IsOpen()) return Fail(TEXT("Porch door failed to open after glass break."));
        if (!State.PorchDoor->Interact(Pawn)) return Fail(TEXT("Broken porch door could not still close normally."));
        State.Stage = 8; State.StageStarted = Now;
    }
    else if (State.Stage == 8)
    {
        if (!State.bPorchFloorAimed && Now-State.StageStarted > 1.2f)
        {
            PC->SetControlRotation((State.PorchPoint - FVector(0,0,80.f)
                - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
            if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
            State.bPorchFloorAimed=true;
        }
        if (!State.bPorchSettledCapture && Now-State.StageStarted > 1.8f)
        {
            CapturePorch(State,TEXT("porch_glass_settled.png"),TEXT("porch_settled_capture"));
            State.bPorchSettledCapture=true;
        }
        if (Now-State.StageStarted <= 2.3f || FScreenshotRequest::IsScreenshotRequested()) return false;
        if (State.PorchDoor->IsOpen() || !State.PorchGlass->IsBroken())
            return Fail(TEXT("Porch door close did not retain its shattered pane."));
        State.Report->SetNumberField(TEXT("porch_shards_resting"),State.PorchGlass->GetRestingShardCount());
        State.Report->SetNumberField(TEXT("porch_shards_supported_by_floor"),State.PorchGlass->GetSupportedShardCount());
        if (State.PorchGlass->GetSupportedShardCount()<25)
            return Fail(TEXT("Real porch glass did not leave enough visible shards supported on the floor."));
        State.Report->SetBoolField(TEXT("porch_opens_closes_after_break"),true);
        TArray<TSharedPtr<FJsonValue>> WindowAudits;
        int32 TestedWindows=0, StandingWindows=0, CrouchingWindows=0, FurnitureObstructedWindows=0;
        const float Radius=Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const float HalfHeight=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const float CrouchedHalfHeight=FMath::Max(Radius,
            Pawn->GetCharacterMovement()->GetCrouchedHalfHeight()*Pawn->GetCapsuleComponent()->GetShapeScale());
        struct FFixedPaneSample
        {
            AHouseReactiveFixture* Window=nullptr;
            FVector Center=FVector::ZeroVector, Normal=FVector::ZeroVector;
            float HalfHeight=0, LateralAllowance=0;
            bool bStanding=false, bBlockedBefore=false;
        };
        TArray<FFixedPaneSample> Samples;
        // Capture every intact positive control before breaking any pane.
        // Some authored openings have two parallel panes in the passage.
        for (TActorIterator<AHouseReactiveFixture> It(World); It; ++It)
        {
            AHouseReactiveFixture* Window=*It;
            if (Window->FixtureKind!=EHouseReactiveFixtureKind::Window || Window->IsBroken()
                || Window->GetOwner() || !Window->OriginalMesh || State.Created.ContainsByPredicate(
                    [Window](const TWeakObjectPtr<AActor>& Actor){return Actor.Get()==Window;})) continue;
            UStaticMeshComponent* Pane=Window->GetFixtureMesh();
            const FBox Bounds=Window->OriginalMesh->GetBoundingBox();
            const FVector Extent=Bounds.GetExtent();
            int32 NormalAxis=Extent.X<Extent.Y?0:1;
            FVector LocalNormal=FVector::ZeroVector;LocalNormal[NormalAxis]=1.f;
            const FVector Normal=Pane->GetComponentTransform().TransformVectorNoScale(LocalNormal).GetSafeNormal();
            const FVector Size=Bounds.GetSize()*Pane->GetComponentScale();
            const float Width=NormalAxis==0?FMath::Abs(Size.Y):FMath::Abs(Size.X);
            if (Width<Radius*2.f+6.f) continue;
            const bool bStandingFits=FMath::Abs(Size.Z)>=HalfHeight*2.f+6.f;
            if (!bStandingFits && FMath::Abs(Size.Z)<CrouchedHalfHeight*2.f+6.f) continue;
            const float PassageHalfHeight=bStandingFits?HalfHeight:CrouchedHalfHeight;
            if (bStandingFits) ++StandingWindows; else ++CrouchingWindows;
            ++TestedWindows;
            FFixedPaneSample Sample;
            Sample.Window=Window;
            Sample.Center=Pane->GetComponentTransform().TransformPosition(Bounds.GetCenter());
            Sample.Normal=Normal;
            Sample.HalfHeight=PassageHalfHeight;
            Sample.LateralAllowance=FMath::Max(0.f,Width*.5f-Radius-3.f);
            Sample.bStanding=bStandingFits;
            FHitResult Before;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(FixedWindowIntactCapsule),false,Pawn);
            Sample.bBlockedBefore=World->SweepSingleByChannel(Before,
                Sample.Center-Normal*95.f,Sample.Center+Normal*95.f,
                FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,PassageHalfHeight),Query);
            if (!Sample.bBlockedBefore || Window->GetFixtureCollision()->GetCollisionEnabled()==ECollisionEnabled::NoCollision
                || Window->GetFixtureCollision()->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block)
                State.ClearanceFailure+=FString::Printf(TEXT(" Intact fixed pane %s did not block the player capsule."),
                    *Window->FixtureId.ToString());
            Samples.Add(Sample);
        }
        // Break both layers before checking their shared passage. Intact
        // neighboring glass retains its production collision until struck.
        for (const FFixedPaneSample& Sample:Samples)
        {
            FHitResult Point;Point.bBlockingHit=true;Point.ImpactPoint=Sample.Center;Point.ImpactNormal=-Sample.Normal;
            for (int32 Hit=0;Hit<3;++Hit)
                UGameplayStatics::ApplyPointDamage(Sample.Window,8.f,Sample.Normal,Point,PC,Pawn,UDamageType::StaticClass());
        }
        for (const FFixedPaneSample& Sample:Samples)
        {
            AHouseReactiveFixture* Window=Sample.Window;
            const FVector Center=Sample.Center,Normal=Sample.Normal;
            const float PassageHalfHeight=Sample.HalfHeight;
            TSharedPtr<FJsonObject> Audit=MakeShared<FJsonObject>();
            Audit->SetStringField(TEXT("fixture_id"),Window->FixtureId.ToString());
            Audit->SetStringField(TEXT("actor"),Window->GetName());
            Audit->SetStringField(TEXT("capsule_posture"),Sample.bStanding?TEXT("standing"):TEXT("crouching"));
            Audit->SetNumberField(TEXT("capsule_halfheight_cm"),PassageHalfHeight);
            Audit->SetBoolField(TEXT("has_breakaway_grille"),Window->BreakawayFrameMesh!=nullptr);
            Audit->SetBoolField(TEXT("intact_blocks_capsule"),Sample.bBlockedBefore);
            Audit->SetStringField(TEXT("pane_center_cm"),Center.ToString());
            TArray<TSharedPtr<FJsonValue>> Neighbors;
            FBox SweepBounds(ForceInit);
            SweepBounds+=Center-Normal*95.f;
            SweepBounds+=Center+Normal*95.f;
            SweepBounds=SweepBounds.ExpandBy(FVector(Radius,Radius,PassageHalfHeight));
            for (const FFixedPaneSample& Other:Samples)
            {
                if (Other.Window==Window || !SweepBounds.Intersect(Other.Window->GetFixtureMesh()->Bounds.GetBox())) continue;
                TSharedPtr<FJsonObject> Neighbor=MakeShared<FJsonObject>();
                Neighbor->SetStringField(TEXT("fixture_id"),Other.Window->FixtureId.ToString());
                Neighbor->SetStringField(TEXT("pane_center_cm"),Other.Center.ToString());
                Neighbor->SetNumberField(TEXT("center_distance_cm"),FVector::Dist(Center,Other.Center));
                Neighbor->SetBoolField(TEXT("also_shattered_for_passage_test"),Other.Window->IsBroken());
                Neighbors.Add(MakeShared<FJsonValueObject>(Neighbor));
            }
            Audit->SetArrayField(TEXT("neighboring_panes_within_sweep_bounds"),Neighbors);
            FHitResult After;
            bool bBlockedAfter=true,bRawPasses=false;
            float PassingOffset=0.f;
            TArray<TSharedPtr<FJsonValue>> CandidateAudits;
            // Keep the authored room layout and production furniture collision.
            // Report each movable obstruction before excluding it from this
            // second query, which isolates permanent window/frame collision.
            TArray<TSharedPtr<FJsonValue>> FurnitureObstacles;
            TSet<AHouseInteractiveProp*> RecordedFurniture;
            const FVector Lateral=FVector::CrossProduct(FVector::UpVector,Normal).GetSafeNormal();
            // A real wall may approach part of the opening from one side. Keep
            // the whole capsule inside the pane and try its remaining width.
            for (float Offset:{0.f,Sample.LateralAllowance,-Sample.LateralAllowance})
            {
                const FVector PassageCenter=Center+Lateral*Offset;
                FCollisionQueryParams Query(SCENE_QUERY_STAT(FixedWindowCapsule),false,Pawn);
                bBlockedAfter=World->SweepSingleByChannel(After,PassageCenter-Normal*95.f,PassageCenter+Normal*95.f,
                    FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,PassageHalfHeight),Query);
                const bool bRawBlocked=bBlockedAfter;
                while (bBlockedAfter)
                {
                    AHouseInteractiveProp* Prop=Cast<AHouseInteractiveProp>(After.GetActor());
                    if (!Prop || !Prop->CanBeMoved()) break;
                    if (!RecordedFurniture.Contains(Prop))
                    {
                        RecordedFurniture.Add(Prop);
                        TSharedPtr<FJsonObject> Obstacle=MakeShared<FJsonObject>();
                        Obstacle->SetStringField(TEXT("actor"),Prop->GetName());
                        Obstacle->SetStringField(TEXT("prop_id"),Prop->PropId.ToString());
                        Obstacle->SetStringField(TEXT("prop_name"),Prop->PropName);
                        Obstacle->SetStringField(TEXT("component"),GetNameSafe(After.GetComponent()));
                        Obstacle->SetBoolField(TEXT("can_be_moved"),true);
                        FurnitureObstacles.Add(MakeShared<FJsonValueObject>(Obstacle));
                    }
                    Query.AddIgnoredActor(Prop);
                    bBlockedAfter=World->SweepSingleByChannel(After,PassageCenter-Normal*95.f,PassageCenter+Normal*95.f,
                        FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,PassageHalfHeight),Query);
                }
                TSharedPtr<FJsonObject> Candidate=MakeShared<FJsonObject>();
                Candidate->SetNumberField(TEXT("lateral_offset_cm"),Offset);
                Candidate->SetStringField(TEXT("capsule_center_cm"),PassageCenter.ToString());
                Candidate->SetBoolField(TEXT("passes_without_moving_furniture"),!bRawBlocked);
                Candidate->SetBoolField(TEXT("permanent_opening_clear"),!bBlockedAfter);
                if (bBlockedAfter)
                {
                    Candidate->SetStringField(TEXT("blocking_actor"),GetNameSafe(After.GetActor()));
                    Candidate->SetStringField(TEXT("blocking_component"),GetNameSafe(After.GetComponent()));
                }
                CandidateAudits.Add(MakeShared<FJsonValueObject>(Candidate));
                if (!bBlockedAfter)
                {
                    PassingOffset=Offset;
                    bRawPasses=!bRawBlocked;
                    break;
                }
            }
            if (!FurnitureObstacles.IsEmpty()) ++FurnitureObstructedWindows;
            Audit->SetBoolField(TEXT("shattered_capsule_passes"),Window->IsBroken()&&bRawPasses);
            Audit->SetArrayField(TEXT("available_lateral_passage_candidates"),CandidateAudits);
            if (!bBlockedAfter) Audit->SetNumberField(TEXT("passing_lateral_offset_cm"),PassingOffset);
            Audit->SetArrayField(TEXT("movable_furniture_obstacles"),FurnitureObstacles);
            Audit->SetBoolField(TEXT("permanent_opening_clear_after_moving_furniture"),Window->IsBroken()&&!bBlockedAfter);
            if (Window->BreakawayFrameMesh && Window->GetBreakawayFrameComponent()->IsVisible())
                State.ClearanceFailure+=FString::Printf(TEXT(" Broken window %s kept its breakaway grille visible."),
                    *Window->FixtureId.ToString());
            if (bBlockedAfter)
            {
                Audit->SetStringField(TEXT("blocking_actor"),GetNameSafe(After.GetActor()));
                Audit->SetStringField(TEXT("blocking_component"),GetNameSafe(After.GetComponent()));
                if (UPrimitiveComponent* Blocker=After.GetComponent())
                {
                    Audit->SetStringField(TEXT("blocking_bounds_center_cm"),Blocker->Bounds.Origin.ToString());
                    Audit->SetStringField(TEXT("blocking_bounds_extent_cm"),Blocker->Bounds.BoxExtent.ToString());
                }
                if (AHouseReactiveFixture* Fixture=Cast<AHouseReactiveFixture>(After.GetActor()))
                {
                    Audit->SetStringField(TEXT("blocking_fixture_id"),Fixture->FixtureId.ToString());
                    Audit->SetBoolField(TEXT("blocking_fixture_broken"),Fixture->IsBroken());
                    Audit->SetStringField(TEXT("blocking_fixture_mesh"),GetNameSafe(Fixture->OriginalMesh));
                }
                State.ClearanceFailure+=FString::Printf(TEXT(" Fixed window %s blocked by %s / %s."),
                    *Window->FixtureId.ToString(),*GetNameSafe(After.GetActor()),*GetNameSafe(After.GetComponent()));
            }
            WindowAudits.Add(MakeShared<FJsonValueObject>(Audit));
        }
        State.Report->SetArrayField(TEXT("fixed_window_capsule_audit"),WindowAudits);
        State.Report->SetNumberField(TEXT("fixed_windows_tested"),TestedWindows);
        State.Report->SetNumberField(TEXT("fixed_windows_standing_tested"),StandingWindows);
        State.Report->SetNumberField(TEXT("fixed_windows_crouching_tested"),CrouchingWindows);
        State.Report->SetNumberField(TEXT("fixed_windows_with_movable_furniture_obstacles"),FurnitureObstructedWindows);
        State.Report->SetBoolField(TEXT("all_capsule_sized_fixed_panes_audited"),true);
        State.Report->SetBoolField(TEXT("all_intact_controls_captured_before_any_fixed_pane_breaks"),true);
        if (!State.ClearanceFailure.IsEmpty()) return Fail(State.ClearanceFailure);
        State.Report->SetBoolField(TEXT("passed"),true);
        OutReport = State.Report;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_FIXTURE_SMOKE_PASSED curtain=1 glass=1 swing=%.3f porch=1"), State.MaxSwing);
        FixtureCleanup(State, Pawn, PC);
        return true;
    }
    return false;
}

void TickHouseGlassSmoke(UWorld* World)
{
    if (!World || !FParse::Param(FCommandLine::Get(), TEXT("HouseGlassSmoke"))
        || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    static bool bFinished = false;
    if (bFinished) return;
    FString UserDir;
    if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || UserDir.IsEmpty()) return;
    APlayerController* PC = World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn = PC ? Cast<AStoryFirstPersonCharacter>(PC->GetPawn()) : nullptr;
    if (!PC || !Pawn || !World->GetGameViewport()) return;
    FString Failure;
    TSharedPtr<FJsonObject> FixtureReport;
    if (!TickHouseFixtureSmoke(World, Pawn, PC, Failure, FixtureReport)) return;
    bFinished = true;
    const bool bPassed = Failure.IsEmpty() && FixtureReport.IsValid();
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("passed"), bPassed);
    Result->SetStringField(TEXT("failure"), Failure);
    if (FixtureReport.IsValid()) Result->SetObjectField(TEXT("fixtures"), FixtureReport);
    FString Evidence;
    FParse::Value(FCommandLine::Get(), TEXT("HouseInteractivityEvidence="), Evidence, false);
    if (Evidence.IsEmpty()) Evidence = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HouseGlassSmoke"));
    IFileManager::Get().MakeDirectory(*Evidence, true);
    FString Json;
    FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Json));
    FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Evidence, TEXT("glass-result.json")));
    UE_LOG(LogTemp, Display, TEXT("HOUSE_GLASS_SMOKE_RESULT passed=%d detail=%s"), bPassed ? 1 : 0, *Failure);
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}
#else
bool TickHouseFixtureSmoke(UWorld*, AStoryFirstPersonCharacter*, APlayerController*, FString&, TSharedPtr<FJsonObject>&)
{
    return true;
}
void TickHouseGlassSmoke(UWorld*) {}
#endif
