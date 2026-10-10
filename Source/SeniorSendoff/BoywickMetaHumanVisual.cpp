#include "BoywickMetaHumanVisual.h"
#include "BoywickCharacter.h"
#include "BoywickAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/LODSyncComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GroomComponent.h"
#include "GroomAsset.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace { const FString Root = TEXT("/Game/MetaHumans/Boywick/MH_Boywick/"); }
ABoywickMetaHumanVisual::ABoywickMetaHumanVisual()
{
    PrimaryActorTick.bCanEverTick = true;
    SetReplicates(false);
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}
bool ABoywickMetaHumanVisual::Initialize(ABoywickCharacter* InCharacter)
{
    if (Native) return IsReady();
    if (!InCharacter || !GetWorld() || GetNetMode() == NM_DedicatedServer) return false;
    Character = InCharacter;
    UClass* Class = LoadClass<AActor>(nullptr, *(Root + TEXT("BP_MH_Boywick.BP_MH_Boywick_C")));
    if (!Class) return false;
    FActorSpawnParameters Params;
    Params.Owner = Character; Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Native = GetWorld()->SpawnActor<AActor>(Class, GetActorTransform(), Params);
    if (!Native) return false;
    Native->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    Native->SetActorRelativeRotation(FRotator(0.f, -90.f, 0.f));
    Native->SetActorEnableCollision(false);
    Native->SetReplicates(false);
    TInlineComponentArray<UPrimitiveComponent*> Primitives(Native);
    for (auto* Component : Primitives)
    {
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
        Component->SetCastShadow(true);
        Component->BoundsScale = 1.2f;
    }
    TInlineComponentArray<USkeletalMeshComponent*> Meshes(Native);
    for (auto* Mesh : Meshes)
    {
        Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        if (Mesh->GetFName() == TEXT("Body")) Body = Mesh;
        if (Mesh->GetFName() == TEXT("Face")) Face = Mesh;
        if (Mesh->GetSkeletalMeshAsset() && Mesh->GetSkeletalMeshAsset()->GetName().Contains(TEXT("Outfits")))
        {
            if (USkeletalMesh* Polo = LoadObject<USkeletalMesh>(nullptr, *(Root + TEXT("Details/SK_Boywick_PoloOutfit.SK_Boywick_PoloOutfit"))))
            {
                Mesh->EmptyOverrideMaterials(); Mesh->SetSkeletalMeshAsset(Polo);
            }
            // Clothing follows the same skeletal pose. Disable expensive cloth
            // simulation for an indoor NPC; native weights keep folds attached.
            Mesh->bDisableClothSimulation = true;
        }
    }
    if (!Body || !Face) return false;
    Idle = LoadObject<UAnimSequence>(nullptr, *(Root + TEXT("Animation/BW_MM_Idle.BW_MM_Idle")));
    Walk = LoadObject<UAnimSequence>(nullptr, *(Root + TEXT("Animation/BW_MF_Unarmed_Walk_Fwd.BW_MF_Unarmed_Walk_Fwd")));
    Jog = LoadObject<UAnimSequence>(nullptr, *(Root + TEXT("Animation/BW_MF_Unarmed_Jog_Fwd.BW_MF_Unarmed_Jog_Fwd")));
    if (!Idle || !Walk || !Jog) return false;
    Body->SetAnimInstanceClass(UBoywickAnimInstance::StaticClass());
    Face->SetLeaderPoseComponent(nullptr, true, true);
    Face->AddTickPrerequisiteComponent(Body);
    Face->SetAnimInstanceClass(UBoywickFaceAnimInstance::StaticClass());
    // Preserve the assembled face's DNA and RigLogic post-process blueprint.
    Face->SetDisablePostProcessBlueprint(false);
    if (ULODSyncComponent* LOD = Native->FindComponentByClass<ULODSyncComponent>())
        LOD->ForcedLOD = 1;
    TInlineComponentArray<UGroomComponent*> Grooms(Native);
    for (auto* Groom : Grooms)
    {
        const FString GroomName=GetNameSafe(Groom->GroomAsset);
        const bool bScalpHair=GroomName.StartsWith(TEXT("Hair_"));
        const bool bSelectedHair=GroomName.Contains(TEXT("SideSweptFringe"));
        Groom->SetUseCards(true);
        Groom->SimulationSettings.bOverrideSettings = true;
        Groom->SetEnableSimulation(false); Groom->ResetSimulation();
        // An assembly rebuild may retain the old palette item's component.
        // Only the selected hairstyle should render, including suffixed slots.
        const bool bUnwanted=(bScalpHair&&!bSelectedHair)||GroomName.StartsWith(TEXT("Beard_"))||GroomName.StartsWith(TEXT("Mustache_"));
        Groom->SetVisibility(!bUnwanted);
        if (bUnwanted) Groom->SetComponentTickEnabled(false);
        if ((bScalpHair&&bSelectedHair)||GroomName.StartsWith(TEXT("Eyebrows_")))
            for (int32 i = 0; i < Groom->GetNumMaterials(); ++i)
                if (auto* Material = Groom->CreateDynamicMaterialInstance(i))
                {
                    Material->SetScalarParameterValue(TEXT("hairMelanin"), .24f);
                    Material->SetScalarParameterValue(TEXT("hairRedness"), .18f);
                    Material->SetScalarParameterValue(TEXT("HairRoughness"), .55f);
                }
        UE_LOG(LogTemp,Display,TEXT("BOYWICK_GROOM component=%s asset=%s visible=%d"),*Groom->GetName(),*GroomName,bUnwanted?0:1);
    }
    Tick(.016f);
    UE_LOG(LogTemp, Display, TEXT("BOYWICK_METAHUMAN_READY body=%s face=%s idle=%s walk=%s jog=%s"),
        *GetNameSafe(Body->GetSkeletalMeshAsset()), *GetNameSafe(Face->GetSkeletalMeshAsset()), *GetNameSafe(Idle), *GetNameSafe(Walk), *GetNameSafe(Jog));
    return IsReady();
}
void ABoywickMetaHumanVisual::Tick(float Delta)
{
    Super::Tick(Delta);
    if (!Character || !Body || !Face) return;
    Clock += Delta;
    Talk = FMath::FInterpTo(Talk, Character->IsTelling() ? 1.f : 0.f, Delta, 4.f);
    Angry = FMath::FInterpTo(Angry, Character->Mood == EBoywickMood::Angry ? 1.f : 0.f, Delta, 5.f);
    Sleep = FMath::FInterpTo(Sleep, Character->Mood == EBoywickMood::KnockedOut ? 1.f : 0.f, Delta, 4.f);
    if (auto* Anim = Cast<UBoywickAnimInstance>(Body->GetAnimInstance()))
    {
        Anim->Idle = Idle; Anim->Walk = Walk; Anim->Jog = Jog;
        Anim->Speed = Character->GetVelocity().Size2D(); Anim->Talking = Talk; Anim->Anger = Angry; Anim->Time = Clock;
        float Yaw = 0.f, Pitch = 0.f;
        if (IsValid(Character->Listener))
        {
            const FVector Direction = Body->GetComponentTransform().InverseTransformVectorNoScale(
                Character->Listener->GetPawnViewLocation() - Body->GetSocketLocation(TEXT("head")));
            Yaw = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Direction.X, Direction.Y)), -28.f, 28.f);
            Pitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Direction.Z, Direction.Size2D())), -15.f, 15.f);
        }
        Anim->HeadYaw = FMath::FInterpTo(Anim->HeadYaw, Yaw, Delta, 4.f);
        Anim->HeadPitch = FMath::FInterpTo(Anim->HeadPitch, Pitch, Delta, 4.f);
    }
    if (auto* Anim = Cast<UBoywickFaceAnimInstance>(Face->GetAnimInstance()))
    {
        Anim->Talking = Talk; Anim->Anger = Angry; Anim->Time = Clock; Anim->Sleep = Sleep;
        // Subtitle timing is the authoritative story clock on server and clients.
        Anim->SpeechPhase = Character->StoryLineElapsed();
    }
}
void ABoywickMetaHumanVisual::EndPlay(const EEndPlayReason::Type Reason)
{
    if (IsValid(Native)) Native->Destroy();
    Native = nullptr;
    Super::EndPlay(Reason);
}
