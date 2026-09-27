#include "SeniorBraxtonVisual.h"
#include "SeniorGarmentAnimInstance.h"
#include "SeniorDouliAnim.h"
#include "SeniorDouliIdle.h"
#include "Animation/AnimSequence.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Components/LODSyncComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "GroomComponent.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "ClothingSimulationInteractor.h"
#include "ChaosCloth/ChaosClothingSimulationInteractor.h"

namespace { const TCHAR* BraxtonRoot = TEXT("/Game/MetaHumans/BraxtonRebuild/MH_Braxton_Rebuild/"); }

ASeniorBraxtonVisual::ASeniorBraxtonVisual()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    SetReplicates(false);
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

bool ASeniorBraxtonVisual::InitializeVisual(bool bPreview, bool bHideFromOwner)
{
    if (NativeCharacter) return Body != nullptr;
    if (!GetWorld() || GetNetMode() == NM_DedicatedServer) return false;
    bIsPreview = bPreview;
    PreviewLeaseUntil = FPlatformTime::Seconds() + 2.0;
    UClass* Class = LoadClass<AActor>(nullptr, *(FString(BraxtonRoot) + TEXT("BP_MH_Braxton_Rebuild.BP_MH_Braxton_Rebuild_C")));
    if (!Class) return false;
    FActorSpawnParameters Params;
    Params.ObjectFlags = RF_Transient;
    Params.Owner = GetOwner() ? GetOwner() : this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    NativeCharacter = GetWorld()->SpawnActor<AActor>(Class, GetActorTransform(), Params);
    if (!NativeCharacter) return false;
    NativeCharacter->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    // MetaHuman is +Y forward; the game and turntable use +X.
    NativeCharacter->SetActorRelativeRotation(FRotator(0,-90,0));
    NativeCharacter->SetActorEnableCollision(false);
    NativeCharacter->SetReplicates(false);
    TInlineComponentArray<UPrimitiveComponent*> Primitives(NativeCharacter);
    for (UPrimitiveComponent* C : Primitives)
    {
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetGenerateOverlapEvents(false);
        // Preview actors live outside the playable map. Keeping the native
        // render proxies visible allows groom/skin-cache updates for captures.
        C->SetVisibleInSceneCaptureOnly(false);
        C->SetOwnerNoSee(bHideFromOwner);
        C->SetCastShadow(true);
        C->bCastHiddenShadow = bHideFromOwner;
        C->BoundsScale = 1.2f;
    }
    TInlineComponentArray<USkeletalMeshComponent*> Meshes(NativeCharacter);
    for (USkeletalMeshComponent* M : Meshes)
    {
        M->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        if (M->GetFName() == TEXT("Body")) Body = M;
        if (M->GetSkeletalMeshAsset() && M->GetSkeletalMeshAsset()->GetName().Contains(TEXT("Outfits")))
        {
            Garments.Add(M);
            USkeletalMesh* Hoodie = LoadObject<USkeletalMesh>(nullptr, *(FString(BraxtonRoot)+TEXT("Details/Hoodie/SK_Braxton_HoodieOutfitV6")));
            if (Hoodie)
            {
                M->SetLeaderPoseComponent(nullptr, true, true);
                M->EmptyOverrideMaterials();
                M->SetSkeletalMeshAsset(Hoodie);
            }
            UMaterialInterface* Shirt = LoadObject<UMaterialInterface>(nullptr, *(FString(BraxtonRoot)+TEXT("Details/M_Braxton_Shirt")));
            UMaterialInterface* Shorts = LoadObject<UMaterialInterface>(nullptr, *(FString(BraxtonRoot)+TEXT("Details/M_Braxton_Short")));
            if (!Hoodie)
                for (int32 I = 0; I < M->GetNumMaterials(); ++I)
                    if (UMaterialInterface* Fabric = I % 2 ? Shirt : Shorts) M->SetMaterial(I,Fabric);
            M->bDisableClothSimulation = false;
            M->SetClothMaxDistanceScale(1.f);
            M->SetComponentTickEnabled(true);
        }
    }
    if (!Body) { NativeCharacter->Destroy(); NativeCharacter=nullptr; return false; }
    Body->AddTickPrerequisiteActor(this);
    Idle = LoadObject<UAnimSequence>(nullptr, *(FString(BraxtonRoot)+TEXT("Animation/BR_MM_Idle.BR_MM_Idle")));
    Walk = LoadObject<UAnimSequence>(nullptr, *(FString(BraxtonRoot)+TEXT("Animation/BR_MF_Unarmed_Walk_Fwd.BR_MF_Unarmed_Walk_Fwd")));
    if (Idle) Body->PlayAnimation(Idle,true);
    for (USkeletalMeshComponent* M : Garments)
    {
        // Copy the body's corrected pose into independent garment buffers,
        // retaining elbow alignment and a separate Chaos cloth simulation.
        M->SetLeaderPoseComponent(nullptr, true, true);
        M->AddTickPrerequisiteComponent(Body);
        M->SetAnimInstanceClass(USeniorGarmentAnimInstance::StaticClass());
        M->bWaitForParallelClothTask = true;
        M->RecreateClothingActors();
        M->ForceClothNextUpdateTeleportAndReset();
        UE_LOG(LogTemp, Display, TEXT("BRAXTON_GARMENT: %s skeleton=%s bodySkeleton=%s"),
            *M->GetName(), *GetNameSafe(M->GetSkeletalMeshAsset()->GetSkeleton()), *GetNameSafe(Body->GetSkeletalMeshAsset()->GetSkeleton()));
    }
    TInlineComponentArray<UGroomComponent*> Grooms(NativeCharacter);
    for (UGroomComponent* G : Grooms)
    {
        G->SetUseCards(bPreview);
        if (G->GetFName() == TEXT("Hair"))
        {
            Hair = G;
            UGroomAsset* Swept = LoadObject<UGroomAsset>(nullptr, *(FString(BraxtonRoot)+TEXT("Details/Hoodie/Hair/SweptFit/Hair_Braxton_Flow")));
            UGroomBindingAsset* Binding = LoadObject<UGroomBindingAsset>(nullptr, *(FString(BraxtonRoot)+TEXT("Details/Hoodie/Hair/SweptFit/Hair_Braxton_Flow_Binding")));
            if (Swept && Binding)
            {
                G->SetGroomAsset(Swept, Binding, true);
                const TArray<FHairGroupsMaterial>& Materials = Swept->GetHairGroupsMaterials();
                for (int32 I=0; I<Materials.Num(); ++I) G->SetMaterial(I, Materials[I].Material);
            }
            G->SetUseCards(bPreview);
            G->SetPhysicsAsset(Body->GetPhysicsAsset());
            G->SimulationSettings.bOverrideSettings = true;
            G->SimulationSettings.SolverSettings.bEnableSimulation = true;
            G->SimulationSettings.SimulationSetup.LocalBone = TEXT("head");
            G->SimulationSettings.SimulationSetup.bLocalSimulation = true;
            G->SimulationSettings.SimulationSetup.AngularVelocityScale = bPreview ? 0.f : .08f;
            G->SimulationSettings.SimulationSetup.LinearVelocityScale = bPreview ? 0.f : .12f;
            G->SimulationSettings.MaterialConstraints.BendStiffness = 2.f;
            G->SimulationSettings.MaterialConstraints.BendDamping = .8f;
            G->SimulationSettings.MaterialConstraints.StretchStiffness = 4.f;
            G->SimulationSettings.MaterialConstraints.StretchDamping = .8f;
            G->SimulationSettings.ExternalForces.AirDrag = .08f;
            // The styled groom already contains its resting gravity shape.
            // Keep a light secondary gravity force instead of letting the fringe
            // collapse a second time over the eyes.
            G->SimulationSettings.ExternalForces.GravityVector = FVector(0,0,-120.f);
            // Turntable manipulation is not physical character movement. Keep
            // the groom's styled rest shape in previews; head animation still
            // drives it. Gameplay retains the damped secondary simulation.
            G->SetEnableSimulation(!bPreview);
            G->ResetSimulation();
            if (bPreview)
            {
                // A turntable hairstyle follows the head rigidly, avoiding
                // delayed skin-cache binding updates during rapid yaw changes.
                G->SetBindingAsset(nullptr);
                G->AttachToComponent(Body, FAttachmentTransformRules::KeepRelativeTransform, TEXT("head"));
                G->SetRelativeTransform(FSeniorDouliIdle::RefBone(Body->GetSkeletalMeshAsset()->GetRefSkeleton(), TEXT("head")).Inverse());
                G->SetForcedLOD(0);
                G->BoundsScale=2.f;
            }
        }
    }
    if (ULODSyncComponent* LOD = NativeCharacter->FindComponentByClass<ULODSyncComponent>())
    {
        LOD->ForcedLOD = bPreview ? 0 : -1;
        if (bPreview)
        {
            // The restored groom has full-detail cards at LOD0. The
            // assembly's medium-hair mapping otherwise forces a stripped LOD.
            LOD->CustomLODMapping.FindOrAdd(TEXT("Hair")).Mapping = {0,0,0,0,0,0,0,0};
            LOD->RefreshSyncComponents();
        }
    }
    LastYaw = GetActorRotation().Yaw;
    LastLocation = GetActorLocation();
    UE_LOG(LogTemp, Display, TEXT("BRAXTON_NATIVE_VISUAL_READY preview=%d garments=%d hair=%d"), bPreview, Garments.Num(), Hair != nullptr);
    return true;
}

void ASeniorBraxtonVisual::KeepPreviewActive()
{
    PreviewLeaseUntil = FPlatformTime::Seconds() + .4;
    SetVisualActive(true);
}

void ASeniorBraxtonVisual::PrestreamPreviewTextures()
{
    if (!NativeCharacter) return;
    TInlineComponentArray<UMeshComponent*> Components(NativeCharacter);
    for (UMeshComponent* C : Components) C->PrestreamTextures(3.f, false);
}

void ASeniorBraxtonVisual::SetMoveSpeed(float Speed)
{
    if (auto* Anim = Body ? Cast<USeniorDouliAnim>(Body->GetAnimInstance()) : nullptr)
    {
        Anim->BaseSequence = Speed > 8 && Walk ? Walk : Idle;
        Anim->Speed = Speed;
        return;
    }
    const bool bNewWalking = bWalking ? Speed > 5.f : Speed > 12.f;
    if (bNewWalking != bWalking && Body)
    {
        bWalking = bNewWalking;
        if (UAnimSequence* Clip = bWalking && Walk ? Walk.Get() : Idle.Get())
        {
            Body->PlayAnimation(Clip,true);
        }
    }
    if (Body) Body->SetPlayRate(bWalking ? FMath::Clamp(Speed / 155.f,.65f,2.1f) : 1.f);
}

void ASeniorBraxtonVisual::SetDouliEquipped(bool bEquipped, float Motion, float FlightBlend)
{
    if (!Body) return;
    if (bEquipped && !Cast<USeniorDouliAnim>(Body->GetAnimInstance()))
        Body->SetAnimInstanceClass(USeniorDouliAnim::StaticClass());
    if (auto* Anim=Cast<USeniorDouliAnim>(Body->GetAnimInstance()))
    {
        if (!Anim->BaseSequence) Anim->BaseSequence=Idle;
        Anim->bEquipped=bEquipped; Anim->Motion=Motion; Anim->FlightBlend=FlightBlend;
        Anim->bLobbyIdle=bIsPreview; Anim->LobbyTime=Time;
    }
    bDouliEquipped=bEquipped;
    if (bIsPreview && bEquipped && !LobbyHat)
    {
        LobbyHat=NewObject<UStaticMeshComponent>(NativeCharacter,TEXT("LobbyDouli"));
        NativeCharacter->AddInstanceComponent(LobbyHat);
        LobbyHat->SetupAttachment(Body,TEXT("hand_r"));
        LobbyHat->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Weapons/Douli/SM_Douli.SM_Douli")));
        LobbyHat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        LobbyHat->RegisterComponent();
    }
    if (LobbyHat)
    {
        LobbyHat->SetVisibility(bEquipped);
        const FSeniorDouliIdle Pose=FSeniorDouliIdle::At(Time);
        const FName Socket=Pose.Attachment==FSeniorDouliIdle::EAttachment::Hand?FName(TEXT("hand_r")):
            (Pose.Attachment==FSeniorDouliIdle::EAttachment::LeftHand?FName(TEXT("hand_l")):
            (Pose.Attachment==FSeniorDouliIdle::EAttachment::Head?FName(TEXT("head")):NAME_None));
        if(LobbyHat->GetAttachSocketName()!=Socket)
            LobbyHat->AttachToComponent(Body,FAttachmentTransformRules::KeepRelativeTransform,Socket);
        const bool Right=Pose.Attachment!=FSeniorDouliIdle::EAttachment::LeftHand;
        const FQuat GripRotation=FSeniorDouliIdle::HandFrame(Body->GetSkeletalMeshAsset()->GetRefSkeleton(),Right)*FSeniorDouliIdle::PalmFrame(Right).Inverse();
        if(Pose.Attachment==FSeniorDouliIdle::EAttachment::Hand || Pose.Attachment==FSeniorDouliIdle::EAttachment::LeftHand)
            LobbyHat->SetRelativeTransform(FTransform(GripRotation,GripRotation.RotateVector(FSeniorDouliIdle::HatOffset(Right))));
        else if(Pose.Attachment==FSeniorDouliIdle::EAttachment::Head)
            LobbyHat->SetRelativeTransform(FSeniorDouliIdle::HeadHat(Body->GetSkeletalMeshAsset()->GetRefSkeleton()));
        else LobbyHat->SetRelativeTransform(Pose.FreeHat);
    }
}

void ASeniorBraxtonVisual::SetVisualActive(bool bActive)
{
    if (bVisualActive == bActive) return;
    bVisualActive = bActive;
    SetActorTickEnabled(bActive);
    if (NativeCharacter)
    {
        NativeCharacter->SetActorHiddenInGame(!bActive);
        TInlineComponentArray<UActorComponent*> Components(NativeCharacter);
        for (UActorComponent* C : Components) C->SetComponentTickEnabled(bActive);
    }
    if (bActive)
    {
        LastYaw=GetActorRotation().Yaw; LastLocation=GetActorLocation(); TurnVelocity=0;
        if (Hair) Hair->ResetSimulation();
        for (USkeletalMeshComponent* M : Garments) M->ForceClothNextUpdateTeleportAndReset();
    }
}

void ASeniorBraxtonVisual::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bIsPreview && FPlatformTime::Seconds() > PreviewLeaseUntil)
    {
        SetVisualActive(false);
        return;
    }
    if (!bVisualActive || !Body || DeltaSeconds <= 0) return;
    const FVector Location = GetActorLocation();
    if (DeltaSeconds > .25f || FVector::DistSquared(Location,LastLocation) > FMath::Square(200.f))
    {
        LastYaw=GetActorRotation().Yaw; TurnVelocity=0;
        if (Hair) Hair->ResetSimulation();
        for (USkeletalMeshComponent* M : Garments) M->ForceClothNextUpdateTeleportAndReset();
    }
    LastLocation = Location;
    const float Dt=FMath::Min(DeltaSeconds,.05f);
    Time += Dt * (bIsPreview ? 1.12f : 1.f);
    if (bIsPreview && bDouliEquipped) SetDouliEquipped(true);
    const float Yaw=GetActorRotation().Yaw;
    const float Angular=FMath::FindDeltaAngleDegrees(LastYaw,Yaw)/FMath::Max(DeltaSeconds,.001f);
    LastYaw=Yaw;
    TurnVelocity=FMath::FInterpTo(TurnVelocity,FMath::Clamp(Angular,-240.f,240.f),Dt,5.f);
    const float Breeze=40.f + 35.f * FMath::Square(FMath::Max(0.f,FMath::Sin(Time*.8f)));
    const FVector Wind(20.f, Breeze + TurnVelocity * .35f, 6.f*FMath::Sin(Time*1.3f));
    if (Hair)
    {
        // A mouse drag is a turntable edit, not a physical gust through the scalp.
        Hair->SimulationSettings.ExternalForces.AirVelocity = FVector(3.f,10.f+4.f*FMath::Sin(Time*.8f),0);
    }
    for (USkeletalMeshComponent* M : Garments)
        if (UClothingSimulationInteractor* Sim = M->GetClothingSimulationInteractor())
            for (const FName Name : {FName(TEXT("BraxtonGarment_0")),FName(TEXT("BraxtonGarment_1"))})
                if (UChaosClothingInteractor* I=Cast<UChaosClothingInteractor>(Sim->GetClothingInteractor(Name)))
                    I->SetWind(FVector2D(.12f,.25f),FVector2D(.02f,.08f),1.225e-6f,Wind);
}

void ASeniorBraxtonVisual::EndPlay(const EEndPlayReason::Type Reason)
{
    if (NativeCharacter) NativeCharacter->Destroy();
    NativeCharacter=nullptr;
    Super::EndPlay(Reason);
}
