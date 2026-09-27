#include "SeniorRunnerVisual.h"
#include "SeniorGarmentAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/LODSyncComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GroomComponent.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
const TCHAR* RunnerRoot = TEXT("/Game/MetaHumans/RunnerRebuild/MH_Runner_Working/");
const TCHAR* RunnerMaleRoot = TEXT("/Game/MetaHumans/RunnerMaleFace/MH_Runner_MaleFace/");
const TCHAR* RunnerCasualRoot = TEXT("/Game/MetaHumans/RunnerCasualHairStudy/MH_Runner_CasualHairStudy/");
const TCHAR* RunnerFaceStudyV4Root = TEXT("/Game/MetaHumans/RunnerFaceStudyV4/MH_Runner_FaceStudyV4/");
const TCHAR* RunnerSamVideoRoot = TEXT("/Game/MetaHumans/RunnerSamVideoHeadStudy/MH_Runner_SamVideoHeadStudy/");
const TCHAR* RunnerSamVideoV2Root = TEXT("/Game/MetaHumans/RunnerSamVideoHeadV2/MH_Runner_SamVideoHeadV2/");
}

ASeniorRunnerVisual::ASeniorRunnerVisual()
{
    PrimaryActorTick.bCanEverTick = true;
    SetReplicates(false);
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

bool ASeniorRunnerVisual::InitializeVisual(bool bInPreview, bool bHideFromOwner)
{
    if (NativeCharacter) return Body != nullptr;
    if (!GetWorld() || GetNetMode() == NM_DedicatedServer) return false;
    bPreview = bInPreview;
    PreviewLeaseUntil = FPlatformTime::Seconds() + 2.0;
    const bool bWorkingFacePreview = FParse::Param(FCommandLine::Get(), TEXT("RunnerWorkingFacePreview"));
    const bool bOriginalHairPreview = FParse::Param(FCommandLine::Get(), TEXT("RunnerOriginalHairPreview"));
    const bool bCasualHairPreview = FParse::Param(FCommandLine::Get(), TEXT("RunnerCasualHairPreview"));
    const bool bSamVideoPreview = FParse::Param(FCommandLine::Get(), TEXT("RunnerSamVideoHeadPreview"));
    const bool bSamVideoV2Preview = FParse::Param(FCommandLine::Get(), TEXT("RunnerSamVideoHeadV2Preview"));
    const bool bLegacyHead = FParse::Param(FCommandLine::Get(), TEXT("RunnerLegacyHead"));
    // The photo-projected face remains available for comparison, but the
    // complete rigged head is the normal character. Mixing a static photo
    // face with a separate animated neck caused visible seams and uncanny
    // proportions in both the lobby and gameplay.
    const bool bVideoMeshPreview = FParse::Param(FCommandLine::Get(), TEXT("RunnerVideoMeshPreview"));
    FString RootPath = (bVideoMeshPreview || bSamVideoV2Preview) ? RunnerSamVideoV2Root : bSamVideoPreview ? RunnerSamVideoRoot : bWorkingFacePreview ? RunnerRoot : bOriginalHairPreview ? RunnerMaleRoot :
        bCasualHairPreview ? RunnerCasualRoot : bLegacyHead ? RunnerFaceStudyV4Root : RunnerRoot;
    UClass* Class = LoadClass<AActor>(nullptr,
        *(RootPath + ((bVideoMeshPreview || bSamVideoV2Preview) ? TEXT("BP_MH_Runner_SamVideoHeadV2.BP_MH_Runner_SamVideoHeadV2_C") :
            bSamVideoPreview ? TEXT("BP_MH_Runner_SamVideoHeadStudy.BP_MH_Runner_SamVideoHeadStudy_C") :
            bWorkingFacePreview ?
            TEXT("BP_MH_Runner_Working.BP_MH_Runner_Working_C") :
            bOriginalHairPreview ? TEXT("BP_MH_Runner_MaleFace.BP_MH_Runner_MaleFace_C") :
            bCasualHairPreview ? TEXT("BP_MH_Runner_CasualHairStudy.BP_MH_Runner_CasualHairStudy_C") :
            bLegacyHead ? TEXT("BP_MH_Runner_FaceStudyV4.BP_MH_Runner_FaceStudyV4_C") :
            TEXT("BP_MH_Runner_Working.BP_MH_Runner_Working_C"))));
    if (!Class && !bVideoMeshPreview && !bSamVideoV2Preview && !bSamVideoPreview && !bWorkingFacePreview && !bOriginalHairPreview && !bCasualHairPreview)
    {
        RootPath = RunnerCasualRoot;
        Class = LoadClass<AActor>(nullptr,
            *(RootPath + TEXT("BP_MH_Runner_CasualHairStudy.BP_MH_Runner_CasualHairStudy_C")));
    }
    if (!Class) return false;
    FActorSpawnParameters Params;
    Params.ObjectFlags = RF_Transient;
    Params.Owner = GetOwner() ? GetOwner() : this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    NativeCharacter = GetWorld()->SpawnActor<AActor>(Class, GetActorTransform(), Params);
    if (!NativeCharacter) return false;
    NativeCharacter->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    // The assembled MetaHuman is +Y forward. The game preview and pawn face +X.
    NativeCharacter->SetActorRelativeRotation(FRotator(0, -90, 0));
    // The source photo contains no ruler; provisional 190 cm art direction.
    NativeCharacter->SetActorRelativeScale3D(FVector(1.09f));
    NativeCharacter->SetActorEnableCollision(false);
    NativeCharacter->SetReplicates(false);
    TInlineComponentArray<UPrimitiveComponent*> Primitives(NativeCharacter);
    for (UPrimitiveComponent* Component : Primitives)
    {
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetOwnerNoSee(bHideFromOwner);
        Component->SetCastShadow(true);
        Component->bCastHiddenShadow = bHideFromOwner;
        Component->BoundsScale = 1.2f;
    }
    TInlineComponentArray<USkeletalMeshComponent*> Meshes(NativeCharacter);
    for (USkeletalMeshComponent* Mesh : Meshes)
    {
        Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        if (Mesh->GetFName() == TEXT("Body")) Body = Mesh;
        if (Mesh->GetSkeletalMeshAsset() && Mesh->GetSkeletalMeshAsset()->GetName().Contains(TEXT("Outfits")))
            Garment = Mesh;
    }
    if (!Body || !Garment)
    {
        NativeCharacter->Destroy(); NativeCharacter = nullptr;
        return false;
    }
    USkeletalMesh* Outfit = nullptr;
    if (!FParse::Param(FCommandLine::Get(), TEXT("RunnerDefaultOutfit")))
    {
        // The factory short-sleeve assembly removes skin hidden under fabric.
        // Use the same face/rig with its complete shoulder mesh for the open
        // armholes of Runner's basketball jersey.
        USkeletalMesh* JerseyBody = LoadObject<USkeletalMesh>(nullptr,
            TEXT("/Game/MetaHumans/RunnerSlimBody/MH_Runner_SlimBody/Body/SKM_MH_Runner_SlimBody_BodyMesh.SKM_MH_Runner_SlimBody_BodyMesh"));
        if (!JerseyBody) JerseyBody = LoadObject<USkeletalMesh>(nullptr,
            TEXT("/Game/MetaHumans/RunnerJerseyBody/MH_Runner_JerseyBody/Body/SKM_MH_Runner_JerseyBody_BodyMesh.SKM_MH_Runner_JerseyBody_BodyMesh"));
        if (!JerseyBody)
        {
            NativeCharacter->Destroy(); NativeCharacter = nullptr;
            return false;
        }
        Body->EmptyOverrideMaterials();
        Body->SetSkeletalMeshAsset(JerseyBody);
        Outfit = LoadObject<USkeletalMesh>(nullptr,
            *(FString(RunnerRoot) + TEXT("Details/RunnerOutfit/SK_Runner_NativeOutfit")));
        if (!Outfit)
        {
            NativeCharacter->Destroy(); NativeCharacter = nullptr;
            return false;
        }
        Garment->SetLeaderPoseComponent(nullptr, true, true);
        Garment->EmptyOverrideMaterials();
        Garment->SetSkeletalMeshAsset(Outfit);
        // The complete native neck fills the V naturally, so the original
        // narrow black-and-gold jersey binding can remain intact.
        Garment->AddTickPrerequisiteComponent(Body);
        Garment->SetAnimInstanceClass(USeniorGarmentAnimInstance::StaticClass());
        Garment->bDisableClothSimulation = false;
        Garment->SetComponentTickEnabled(true);
    }

    Idle = LoadObject<UAnimSequence>(nullptr,
        *(FString(RunnerRoot) + TEXT("Animation/RN_MM_Idle.RN_MM_Idle")));
    Walk = LoadObject<UAnimSequence>(nullptr,
        *(FString(RunnerRoot) + TEXT("Animation/RN_MF_Unarmed_Walk_Fwd.RN_MF_Unarmed_Walk_Fwd")));
    if (Idle) Body->PlayAnimation(Idle, true);
    if (FParse::Param(FCommandLine::Get(), TEXT("RunnerShowSunglasses")))
    {
        if (UStaticMesh* Frames = LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/MetaHumans/RunnerRebuild/MH_Runner_Working/Details/RunnerOutfit/SM_RunnerSunglasses.SM_RunnerSunglasses")))
        {
            UStaticMeshComponent* Glasses = NewObject<UStaticMeshComponent>(NativeCharacter, TEXT("RunnerSunglasses"));
            Glasses->SetStaticMesh(Frames);
            Glasses->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Glasses->SetOwnerNoSee(bHideFromOwner);
            Glasses->RegisterComponent();
            Glasses->SetWorldTransform(Body->GetComponentTransform());
            Glasses->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform, TEXT("head"));
            // The source frames sat in front of the nose on the fitted face.
            // Bring the lenses back onto the bridge while retaining their eye level.
            Glasses->SetWorldLocation(Glasses->GetComponentLocation() +
                Body->GetComponentTransform().TransformVectorNoScale(FVector(0, -4.f, 0)) +
                FVector(0, 0, 16.f));
        }
    }
    for (const TCHAR* Side : {TEXT("L"), TEXT("R")})
    {
        const FString Name = FString::Printf(TEXT("SM_RunnerSandal%s"), Side);
        const FString AssetPath = FString(RunnerRoot) + TEXT("Details/RunnerOutfit/") + Name + TEXT(".") + Name;
        UStaticMesh* Sandal = LoadObject<UStaticMesh>(nullptr, *AssetPath);
        if (!Sandal) continue;
        const TCHAR* Bone = FCString::Strcmp(Side, TEXT("L")) == 0 ? TEXT("foot_l") : TEXT("foot_r");
        UStaticMeshComponent* Footwear = NewObject<UStaticMeshComponent>(NativeCharacter, *Name);
        Footwear->SetStaticMesh(Sandal);
        Footwear->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Footwear->SetOwnerNoSee(bHideFromOwner);
        Footwear->RegisterComponent();
        Footwear->SetWorldTransform(Body->GetComponentTransform());
        Footwear->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform, Bone);
        const float Inward = FCString::Strcmp(Side, TEXT("L")) == 0 ? -4.f : 4.f;
        Footwear->SetWorldLocation(Footwear->GetComponentLocation() +
            Body->GetComponentTransform().TransformVectorNoScale(FVector(Inward, 0, 0)) + FVector(0, 0, 2.5f));
    }
    float FaceLiftZ = bVideoMeshPreview ? -4.f : 0.f;
    FParse::Value(FCommandLine::Get(), TEXT("RunnerFaceLift="), FaceLiftZ);
    if (FaceLiftZ != 0.f)
    {
        for (USkeletalMeshComponent* Mesh : Meshes)
            if (Mesh->GetFName() == TEXT("Face")) Mesh->AddLocalOffset(FVector(0, 0, FaceLiftZ));
    }
    TInlineComponentArray<UGroomComponent*> Grooms(NativeCharacter);
    for (UGroomComponent* Groom : Grooms)
    {
        if (bVideoMeshPreview && Groom->GetFName() != TEXT("Hair"))
        {
            Groom->SetVisibility(false);
            continue;
        }
        Groom->SetUseCards(true);
        if (Groom->GetFName() == TEXT("Hair"))
        {
            // Keep the hairstyle bound to Runner's own face. The other
            // character grooms do not share this scalp fit.
            Groom->SimulationSettings.bOverrideSettings = true;
            Groom->SetEnableSimulation(false);
            Groom->ResetSimulation();
            // The complete MetaHuman face supports the fuller fringe from the
            // video study without the disconnected photo-projection shell.
            if (RootPath == RunnerRoot || FParse::Param(FCommandLine::Get(), TEXT("RunnerLongFringe")))
            {
                UGroomAsset* Fringe = LoadObject<UGroomAsset>(nullptr,
                    TEXT("/Game/MetaHumans/RunnerSamVideoHeadV2/MH_Runner_SamVideoHeadV2/Grooms/Hair_S_SideSweptFringe.Hair_S_SideSweptFringe"));
                UGroomBindingAsset* FringeBinding = LoadObject<UGroomBindingAsset>(nullptr,
                    TEXT("/Game/MetaHumans/RunnerSamVideoHeadV2/MH_Runner_SamVideoHeadV2/Grooms/Hair_S_SideSweptFringe_Binding.Hair_S_SideSweptFringe_Binding"));
                if (Fringe && FringeBinding) Groom->SetGroomAsset(Fringe, FringeBinding, false);
            }
            // Individual strands preserve the fringe's soft outline at lobby
            // close-up distance. Keep cards in gameplay and on mobile renderers.
            const bool bPreviewStrands = bPreview && GetWorld()->GetFeatureLevel() >= ERHIFeatureLevel::SM5 &&
                !FParse::Param(FCommandLine::Get(), TEXT("RunnerHairCards"));
            Groom->SetUseCards(!bPreviewStrands);
            float HairMelanin = .55f;
            float HairRedness = .82f;
            float HairRoughness = .65f;
            const bool bMelaninOverride = FParse::Value(FCommandLine::Get(), TEXT("RunnerHairMelanin="), HairMelanin);
            const bool bRednessOverride = FParse::Value(FCommandLine::Get(), TEXT("RunnerHairRedness="), HairRedness);
            const bool bRoughnessOverride = FParse::Value(FCommandLine::Get(), TEXT("RunnerHairRoughness="), HairRoughness);
            const bool bTuneHair = RootPath == RunnerRoot || (bMelaninOverride && bRednessOverride);
            const bool bTuneRoughness = RootPath == RunnerRoot || bRoughnessOverride;
            if (bTuneHair || bTuneRoughness)
            {
                for (int32 Index = 0; Index < Groom->GetNumMaterials(); ++Index)
                {
                    if (UMaterialInstanceDynamic* HairMaterial = Groom->CreateDynamicMaterialInstance(Index))
                    {
                        if (bTuneHair)
                        {
                            HairMaterial->SetScalarParameterValue(TEXT("hairMelanin"), HairMelanin);
                            HairMaterial->SetScalarParameterValue(TEXT("hairRedness"), HairRedness);
                        }
                        if (bTuneRoughness) HairMaterial->SetScalarParameterValue(TEXT("HairRoughness"), HairRoughness);
                    }
                }
            }
            if (bVideoMeshPreview)
            {
                // Keep the original bound hair placement for the video head.
                float GroomScale = 1.f;
                float GroomDrop = 0.f;
                FParse::Value(FCommandLine::Get(), TEXT("RunnerGroomScale="), GroomScale);
                FParse::Value(FCommandLine::Get(), TEXT("RunnerGroomDrop="), GroomDrop);
                Groom->SetWorldScale3D(Groom->GetComponentScale() * GroomScale);
                Groom->AddWorldOffset(FVector(0, 0, GroomDrop));
            }
            UE_LOG(LogTemp, Display, TEXT("RUNNER_HAIR_ASSET: %s"), *GetNameSafe(Groom->GroomAsset));
        }
    }
    if (bVideoMeshPreview)
    {
        for (USkeletalMeshComponent* Mesh : Meshes)
            if (Mesh->GetFName() == TEXT("Face"))
            {
                // Keep the assembled rig alive, but draw only the replacement
                // head. The prior facial features do not match the reference.
                Mesh->SetRenderInMainPass(false);
                Mesh->SetRenderInDepthPass(false);
                Mesh->SetCastShadow(false);
            }
        UStaticMesh* HeadMesh = LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/Characters/MetaHumans/Runner/VideoHeadVisual/SM_Runner_SamVideoHeadVisualV2.SM_Runner_SamVideoHeadVisualV2"));
        UStaticMesh* NeckMesh = LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/Characters/MetaHumans/Runner/VideoHeadVisual/SM_Runner_SamVideoCollarBlend.SM_Runner_SamVideoCollarBlend"));
        UMaterialInterface* HeadMaterial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Characters/MetaHumans/Runner/VideoHeadVisual/M_RunnerSamVideoHeadBalanced.M_RunnerSamVideoHeadBalanced"));
        UMaterialInterface* NeckMaterial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Characters/MetaHumans/Runner/VideoHeadVisual/M_RunnerSamVideoNeckV2.M_RunnerSamVideoNeckV2"));
        if (!HeadMesh || !NeckMesh || !HeadMaterial || !NeckMaterial)
        {
            NativeCharacter->Destroy(); NativeCharacter = nullptr;
            return false;
        }
        // Restore the single fitted head and neck that predated this pass.
        float HeadScale = 0.62f;
        constexpr float HeadPivotZ = 172.f;
        float HeadLiftZ = 10.f;
        FParse::Value(FCommandLine::Get(), TEXT("RunnerHeadScale="), HeadScale);
        FParse::Value(FCommandLine::Get(), TEXT("RunnerHeadLift="), HeadLiftZ);
        const auto AttachHeadPart = [this, bHideFromOwner, HeadLiftZ](UStaticMesh* Mesh, const TCHAR* Name, float Scale)
        {
            UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(NativeCharacter, Name);
            Part->SetStaticMesh(Mesh);
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Part->SetOwnerNoSee(bHideFromOwner);
            Part->RegisterComponent();
            Part->SetWorldTransform(Body->GetComponentTransform());
            Part->SetWorldScale3D(Body->GetComponentScale() * Scale);
            Part->SetWorldLocation(Body->GetComponentLocation() +
                Body->GetComponentTransform().TransformVectorNoScale(
                    FVector(0, 0, HeadPivotZ * (1.f - Scale) + HeadLiftZ)));
            Part->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform, TEXT("head"));
            return Part;
        };
        UStaticMeshComponent* VideoHead = AttachHeadPart(HeadMesh, TEXT("RunnerSamVideoHead"), HeadScale);
        VideoHead->SetMaterial(0, HeadMaterial);
        UStaticMeshComponent* Neck = AttachHeadPart(NeckMesh, TEXT("RunnerSamVideoNeck"), HeadScale);
        Neck->SetMaterial(0, NeckMaterial);
        UE_LOG(LogTemp, Display, TEXT("RUNNER_VIDEO_MESH_ATTACHED: %s"), *GetNameSafe(HeadMesh));
    }
    if (ULODSyncComponent* LOD = NativeCharacter->FindComponentByClass<ULODSyncComponent>())
        LOD->ForcedLOD = bPreview ? 0 : -1;
    UE_LOG(LogTemp, Display, TEXT("RUNNER_NATIVE_VISUAL_READY preview=%d source=%s outfit=%s heightArtCm=190"),
        bPreview, *RootPath, *GetNameSafe(Outfit));
    return true;
}

void ASeniorRunnerVisual::KeepPreviewActive()
{
    PreviewLeaseUntil = FPlatformTime::Seconds() + .4;
    SetVisualActive(true);
}

void ASeniorRunnerVisual::SetVisualActive(bool bNowActive)
{
    if (bActive == bNowActive) return;
    bActive = bNowActive;
    SetActorTickEnabled(bActive);
    if (NativeCharacter)
    {
        NativeCharacter->SetActorHiddenInGame(!bActive);
        TInlineComponentArray<UActorComponent*> Components(NativeCharacter);
        for (UActorComponent* Component : Components) Component->SetComponentTickEnabled(bActive);
    }
}

void ASeniorRunnerVisual::PrestreamPreviewTextures()
{
    if (!NativeCharacter) return;
    TInlineComponentArray<UMeshComponent*> Components(NativeCharacter);
    for (UMeshComponent* Component : Components) Component->PrestreamTextures(3.f, false);
}

void ASeniorRunnerVisual::SetMoveSpeed(float Speed)
{
    const bool bShouldWalk = bWalking ? Speed > 5.f : Speed > 12.f;
    if (bShouldWalk != bWalking && Body)
    {
        bWalking = bShouldWalk;
        if (UAnimSequence* Clip = bWalking && Walk ? Walk.Get() : Idle.Get())
            Body->PlayAnimation(Clip, true);
    }
    if (Body) Body->SetPlayRate(bWalking ? FMath::Clamp(Speed / 155.f, .65f, 2.1f) : 1.f);
}

void ASeniorRunnerVisual::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bPreview && FPlatformTime::Seconds() > PreviewLeaseUntil)
        SetVisualActive(false);
}

void ASeniorRunnerVisual::EndPlay(const EEndPlayReason::Type Reason)
{
    if (NativeCharacter) NativeCharacter->Destroy();
    NativeCharacter = nullptr;
    Super::EndPlay(Reason);
}
