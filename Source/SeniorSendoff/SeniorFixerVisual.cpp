#include "SeniorFixerVisual.h"
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
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
const TCHAR* FixerRoot = TEXT("/Game/MetaHumans/Fixer/MH_Fixer/");
FTransform FixerReferenceBone(const FReferenceSkeleton& Reference, const FName BoneName)
{
    FTransform Transform = FTransform::Identity;
    for (int32 Bone = Reference.FindBoneIndex(BoneName); Bone != INDEX_NONE; Bone = Reference.GetParentIndex(Bone))
        Transform *= Reference.GetRefBonePose()[Bone];
    return Transform;
}
}

ASeniorFixerVisual::ASeniorFixerVisual()
{
    PrimaryActorTick.bCanEverTick = true;
    SetReplicates(false);
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

bool ASeniorFixerVisual::InitializeVisual(bool bInPreview, bool bHideFromOwner)
{
    if (NativeCharacter) return Body != nullptr;
    if (!GetWorld() || GetNetMode() == NM_DedicatedServer) return false;
    bPreview = bInPreview;
    PreviewLeaseUntil = FPlatformTime::Seconds() + 2.0;
    UClass* CharacterClass = LoadClass<AActor>(nullptr,
        TEXT("/Game/MetaHumans/FixerLean/MH_Fixer_Lean/BP_MH_Fixer_Lean.BP_MH_Fixer_Lean_C"));
    if (!CharacterClass) CharacterClass = LoadClass<AActor>(nullptr,
        *(FString(FixerRoot) + TEXT("BP_MH_Fixer.BP_MH_Fixer_C")));
    if (!CharacterClass) return false;
    FActorSpawnParameters Params;
    Params.ObjectFlags = RF_Transient;
    Params.Owner = GetOwner() ? GetOwner() : this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    NativeCharacter = GetWorld()->SpawnActor<AActor>(CharacterClass, GetActorTransform(), Params);
    if (!NativeCharacter) return false;
    NativeCharacter->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    // The assembly is +Y forward and authored at 168 cm. Preserve its slim
    // proportions instead of stretching a taller body to fit the game capsule.
    NativeCharacter->SetActorRelativeRotation(FRotator(0, -90, 0));
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
    USkeletalMesh* LeanBody = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/MetaHumans/FixerLean/MH_Fixer_Lean/Body/SKM_MH_Fixer_Lean_BodyMesh.SKM_MH_Fixer_Lean_BodyMesh"));
    USkeletalMesh* LeanOutfit = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/MetaHumans/FixerLean/MH_Fixer_Lean/Clothing/MH_Fixer_Lean_Outfits.MH_Fixer_Lean_Outfits"));
    const bool bLeanFit = LeanBody && LeanOutfit;
    USkeletalMeshComponent* NativeGarment = nullptr;
    UMaterialInterface* Shirt = LoadObject<UMaterialInterface>(nullptr,
        *(FString(FixerRoot) + TEXT("Details/M_Fixer_Shirt.M_Fixer_Shirt")));
    UMaterialInterface* Shorts = LoadObject<UMaterialInterface>(nullptr,
        *(FString(FixerRoot) + TEXT("Details/M_Fixer_Short.M_Fixer_Short")));
    for (USkeletalMeshComponent* Mesh : Meshes)
    {
        Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        if (Mesh->GetFName() == TEXT("Body"))
        {
            Body = Mesh;
            if (bLeanFit) Body->SetSkeletalMeshAsset(LeanBody);
        }
        if (Mesh->GetFName() == TEXT("Face") && Mesh->GetSkeletalMeshAsset())
        {
            // The new asset retains fair skin and independently tuned brown eyes.
            const auto& FaceMaterials=Mesh->GetSkeletalMeshAsset()->GetMaterials();
            for (int32 Slot=0; Slot<FaceMaterials.Num(); ++Slot)
                Mesh->SetMaterial(Slot,FaceMaterials[Slot].MaterialInterface);
        }
        USkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
        if (Asset && Asset->GetName().Contains(TEXT("Outfits")))
        {
            NativeGarment = Mesh;
            if (bLeanFit) { Mesh->SetSkeletalMeshAsset(LeanOutfit); Asset=LeanOutfit; }
            for (int32 Slot = 0; Slot < Asset->GetMaterials().Num(); ++Slot)
            {
                const FString SlotName = Asset->GetMaterials()[Slot].MaterialSlotName.ToString();
                // The assembly Blueprint stores its own white material overrides.
                // Set the fitted garment's named slots explicitly at runtime.
                UMaterialInterface* Fabric = SlotName.Contains(TEXT("Shirt")) ? Shirt :
                    SlotName.Contains(TEXT("Short")) ? Shorts : nullptr;
                if (Fabric) Mesh->SetMaterial(Slot, Fabric);
            }
        }
    }
    if (!Body)
    {
        NativeCharacter->Destroy();
        NativeCharacter = nullptr;
        return false;
    }
    Body->AddTickPrerequisiteActor(this);
    const FString AnimationRoot = bLeanFit ? TEXT("/Game/MetaHumans/FixerLean/MH_Fixer_Lean/") : FixerRoot;
    Idle = LoadObject<UAnimSequence>(nullptr,
        *(AnimationRoot + TEXT("Animation/FX_MM_Idle.FX_MM_Idle")));
    Walk = LoadObject<UAnimSequence>(nullptr,
        *(AnimationRoot + TEXT("Animation/FX_MF_Unarmed_Walk_Fwd.FX_MF_Unarmed_Walk_Fwd")));
    if (Idle) Body->PlayAnimation(Idle, true);
    if (USkeletalMesh* EarbudsAsset=LoadObject<USkeletalMesh>(nullptr,
        *(FString(FixerRoot)+TEXT("Details/SK_FixerWiredEarbuds.SK_FixerWiredEarbuds"))))
    {
        USkeletalMeshComponent* Earbuds=NewObject<USkeletalMeshComponent>(NativeCharacter,TEXT("FixerWiredEarbuds"));
        NativeCharacter->AddInstanceComponent(Earbuds);
        Earbuds->SetupAttachment(Body);
        Earbuds->SetRelativeTransform(FTransform::Identity);
        Earbuds->SetSkeletalMeshAsset(EarbudsAsset);
        Earbuds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Earbuds->SetGenerateOverlapEvents(false);
        Earbuds->SetOwnerNoSee(bHideFromOwner);
        Earbuds->SetCastShadow(true);
        Earbuds->bCastHiddenShadow=bHideFromOwner;
        Earbuds->bUseAttachParentBound=true;
        Earbuds->RegisterComponent();
        Earbuds->SetLeaderPoseComponent(Body,true,false);
        Earbuds->AddTickPrerequisiteComponent(Body);
        UE_LOG(LogTemp,Display,TEXT("FIXER_WIRED_EARBUDS_ATTACHED"));
    }
    if (USkeletalMesh* PrintAsset = LoadObject<USkeletalMesh>(nullptr,
        *(FString(FixerRoot) + (bLeanFit ?
            TEXT("Details/SK_FixerLeanShirtPrints.SK_FixerLeanShirtPrints") :
            TEXT("Details/SK_FixerShirtPrints.SK_FixerShirtPrints")))))
    {
        USkeletalMeshComponent* Prints = NewObject<USkeletalMeshComponent>(NativeCharacter, TEXT("FixerShirtPrints"));
        NativeCharacter->AddInstanceComponent(Prints);
        USkeletalMeshComponent* PrintDriver = NativeGarment ? NativeGarment : Body.Get();
        Prints->SetupAttachment(PrintDriver);
        Prints->SetRelativeTransform(FTransform::Identity);
        Prints->SetSkeletalMeshAsset(PrintAsset);
        Prints->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Prints->SetGenerateOverlapEvents(false);
        Prints->SetOwnerNoSee(bHideFromOwner);
        Prints->SetCastShadow(false);
        Prints->bUseAttachParentBound = true;
        Prints->RegisterComponent();
        // The appliques are projected and weighted from the fitted outfit.
        // Follow its post-processed garment pose as well as its bind skeleton.
        Prints->SetLeaderPoseComponent(PrintDriver, true, false);
        Prints->AddTickPrerequisiteComponent(PrintDriver);
        UE_LOG(LogTemp, Display, TEXT("FIXER_SHIRT_PRINTS_ATTACHED: %s driver=%s"),
            *PrintAsset->GetName(), *PrintDriver->GetName());
    }
    UStaticMesh* SneakerLeft = LoadObject<UStaticMesh>(nullptr,
        *(FString(FixerRoot) + TEXT("Details/SM_FixerSneakerBody_L.SM_FixerSneakerBody_L")));
    UStaticMesh* SneakerRight = LoadObject<UStaticMesh>(nullptr,
        *(FString(FixerRoot) + TEXT("Details/SM_FixerSneakerBody_R.SM_FixerSneakerBody_R")));
    const bool bBodySpaceSneakers = SneakerLeft && SneakerRight;
    USkeletalMesh* SneakerReference = LoadObject<USkeletalMesh>(nullptr,
        *(FString(FixerRoot)+TEXT("Body/SKM_MH_Fixer_BodyMesh.SKM_MH_Fixer_BodyMesh")));
    if (!bBodySpaceSneakers)
    {
        SneakerLeft = LoadObject<UStaticMesh>(nullptr,
            *(FString(FixerRoot) + TEXT("Details/SM_FixerSneaker_L.SM_FixerSneaker_L")));
        SneakerRight = LoadObject<UStaticMesh>(nullptr,
            *(FString(FixerRoot) + TEXT("Details/SM_FixerSneaker_R.SM_FixerSneaker_R")));
    }
    if (SneakerLeft && SneakerRight && Body->GetBoneIndex(TEXT("foot_l")) != INDEX_NONE &&
        Body->GetBoneIndex(TEXT("foot_r")) != INDEX_NONE)
    {
        const auto AttachSneaker = [this, bHideFromOwner, bBodySpaceSneakers, SneakerReference](UStaticMesh* Asset, const TCHAR* Name, const TCHAR* Bone)
        {
            UStaticMeshComponent* Shoe = NewObject<UStaticMeshComponent>(NativeCharacter, Name);
            NativeCharacter->AddInstanceComponent(Shoe);
            Shoe->SetStaticMesh(Asset);
            Shoe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Shoe->SetGenerateOverlapEvents(false);
            Shoe->SetOwnerNoSee(bHideFromOwner);
            Shoe->SetCastShadow(true);
            Shoe->bCastHiddenShadow = bHideFromOwner;
            Shoe->SetupAttachment(Body, Bone);
            // Preferred meshes preserve native body coordinates through FBX.
            // Convert once from body bind space to the animated foot socket.
            // The older foot-local exports remain an optional identity fallback.
            Shoe->SetRelativeTransform(bBodySpaceSneakers ?
                FixerReferenceBone((SneakerReference ? SneakerReference : Body->GetSkeletalMeshAsset())->GetRefSkeleton(), Bone).Inverse() : FTransform::Identity);
            Shoe->RegisterComponent();
        };
        AttachSneaker(SneakerLeft, TEXT("FixerSneakerLeft"), TEXT("foot_l"));
        AttachSneaker(SneakerRight, TEXT("FixerSneakerRight"), TEXT("foot_r"));
        const auto IsFootwear = [](const FString& Name)
        {
            return Name.Contains(TEXT("Shoe")) || Name.Contains(TEXT("Sneaker")) ||
                Name.Contains(TEXT("Footwear")) || Name.Contains(TEXT("Sandal"));
        };
        // Replace native footwear only once both new shoes are present. Keep
        // the body's foot bones and skin intact: hiding a bone scales its
        // socket to zero and would also collapse the attached replacement.
        for (UPrimitiveComponent* Component : Primitives)
            if (Component != Body && IsFootwear(Component->GetName())) Component->SetVisibility(false);
        for (USkeletalMeshComponent* Mesh : Meshes)
        {
            USkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
            if (Mesh == Body || !Asset) continue;
            for (int32 Slot = 0; Slot < Asset->GetMaterials().Num(); ++Slot)
            {
                const FSkeletalMaterial& Material = Asset->GetMaterials()[Slot];
                if (!IsFootwear(Material.MaterialSlotName.ToString()) && !IsFootwear(GetNameSafe(Material.MaterialInterface))) continue;
                for (int32 LOD = 0; LOD < Asset->GetLODNum(); ++LOD)
                    Mesh->ShowMaterialSection(Slot, INDEX_NONE, false, LOD);
                UE_LOG(LogTemp, Display, TEXT("FIXER_NATIVE_FOOTWEAR_HIDDEN mesh=%s slot=%s"),
                    *Mesh->GetName(), *Material.MaterialSlotName.ToString());
            }
        }
        UE_LOG(LogTemp, Display, TEXT("FIXER_SNEAKERS_ATTACHED bodySpace=%d"), bBodySpaceSneakers);
    }
    TInlineComponentArray<UGroomComponent*> Grooms(NativeCharacter);
    for (UGroomComponent* Groom : Grooms)
    {
        if (Groom->GetFName() == TEXT("Hair"))
        {
            UGroomAsset* Mullet = LoadObject<UGroomAsset>(nullptr,
                *(FString(FixerRoot)+TEXT("Details/Hair/Mullet/Hair_Fixer_ShortMulletV2.Hair_Fixer_ShortMulletV2")));
            UGroomBindingAsset* MulletBinding = LoadObject<UGroomBindingAsset>(nullptr,
                *(FString(FixerRoot)+TEXT("Details/Hair/Mullet/Hair_Fixer_ShortMulletV2_LeanBinding.Hair_Fixer_ShortMulletV2_LeanBinding")));
            UGroomAsset* Fringe = LoadObject<UGroomAsset>(nullptr,
                *(FString(FixerRoot) + TEXT("Details/Hair/Hair_Fixer_Fringe.Hair_Fixer_Fringe")));
            UGroomBindingAsset* FringeBinding = LoadObject<UGroomBindingAsset>(nullptr,
                *(FString(FixerRoot) + TEXT("Details/Hair/Hair_Fixer_Fringe_BindingV2.Hair_Fixer_Fringe_BindingV2")));
            if (Mullet && MulletBinding) Groom->SetGroomAsset(Mullet, MulletBinding, true);
            else if (Fringe && FringeBinding) Groom->SetGroomAsset(Fringe, FringeBinding, true);
        }
        if (Groom->GroomAsset)
        {
            const TArray<FHairGroupsMaterial>& Materials = Groom->GroomAsset->GetHairGroupsMaterials();
            for (int32 Slot = 0; Slot < Materials.Num(); ++Slot)
                if (Materials[Slot].Material) Groom->SetMaterial(Slot, Materials[Slot].Material);
        }
        // All grooms retain their own fitted binding and authored ginger
        // materials. Turntable rotation cannot fling the hairstyle off the scalp.
        Groom->SimulationSettings.bOverrideSettings = true;
        Groom->SetEnableSimulation(false);
        Groom->ResetSimulation();
        // The restored fitted groom supplies full-detail cards for the Mac
        // preview and mobile renderer; its optimized strands may be absent.
        Groom->SetUseCards(true);
        if (bPreview && Groom->GetFName() == TEXT("Hair")) Groom->SetForcedLOD(0);
    }
    if (ULODSyncComponent* LOD = NativeCharacter->FindComponentByClass<ULODSyncComponent>())
    {
        LOD->ForcedLOD = bPreview ? 0 : -1;
        if (bPreview)
        {
            LOD->CustomLODMapping.FindOrAdd(TEXT("Hair")).Mapping = {0,0,0,0,0,0,0,0};
            LOD->RefreshSyncComponents();
        }
    }
    UE_LOG(LogTemp, Display, TEXT("FIXER_NATIVE_VISUAL_READY preview=%d heightCm=168 idle=%s walk=%s"),
        bPreview, *GetNameSafe(Idle), *GetNameSafe(Walk));
    return true;
}

void ASeniorFixerVisual::KeepPreviewActive()
{
    PreviewLeaseUntil = FPlatformTime::Seconds() + .4;
    SetVisualActive(true);
}

void ASeniorFixerVisual::SetVisualActive(bool bNowActive)
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

void ASeniorFixerVisual::PrestreamPreviewTextures()
{
    if (!NativeCharacter) return;
    TInlineComponentArray<UMeshComponent*> Components(NativeCharacter);
    for (UMeshComponent* Component : Components) Component->PrestreamTextures(3.f, false);
}

void ASeniorFixerVisual::SetMoveSpeed(float Speed)
{
    const bool bShouldWalk = bWalking ? Speed > 5.f : Speed > 12.f;
    if (bShouldWalk != bWalking && Body)
    {
        bWalking = bShouldWalk;
        if (UAnimSequence* Clip = bWalking && Walk ? Walk.Get() : Idle.Get()) Body->PlayAnimation(Clip, true);
    }
    if (Body) Body->SetPlayRate(bWalking ? FMath::Clamp(Speed / 145.f, .65f, 2.1f) : 1.f);
}

void ASeniorFixerVisual::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bPreview && FPlatformTime::Seconds() > PreviewLeaseUntil) SetVisualActive(false);
}

void ASeniorFixerVisual::EndPlay(const EEndPlayReason::Type Reason)
{
    if (NativeCharacter) NativeCharacter->Destroy();
    NativeCharacter = nullptr;
    Super::EndPlay(Reason);
}
