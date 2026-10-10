// Boywick's hooks into the story player: focus, E interaction and the timed blessings he hands out.
#include "StoryCampaign.h"
#include "BoywickCharacter.h"
#include "HouseFurnitureInteractionComponent.h"
#include "StoryMovementComponent.h"
#include "SeniorDouli.h"
#include "SeniorSauce.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"

namespace
{
float BoywickServerNow(const UWorld* World)
{
    if (!World) return 0.f;
    if (const AGameStateBase* GS = World->GetGameState()) return GS->GetServerWorldTimeSeconds();
    return World->GetTimeSeconds();
}
}

ABoywickCharacter* AStoryFirstPersonCharacter::FocusedBoywick() const
{
    if (!GetWorld() || !Controller || CarriedHousePig()
        || (FurnitureInteraction && FurnitureInteraction->bHoldingFurniture)) return nullptr;
    if (const APlayerController* PC = Cast<APlayerController>(Controller); PC && PC->bShowMouseCursor) return nullptr;
    FVector View; FRotator Rotation; Controller->GetPlayerViewPoint(View, Rotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BoywickFocus), false, this);
    if (Douli) Query.AddIgnoredActor(Douli.Get());
    if (SaucePacket) Query.AddIgnoredActor(SaucePacket.Get());
    const FVector End = View + Rotation.Vector() * 340.f;
    FHitResult Hit;
    GetWorld()->LineTraceSingleByChannel(Hit, View, End, ECC_Visibility, Query);
    if (ABoywickCharacter* Boywick = Cast<ABoywickCharacter>(Hit.GetActor())) return Boywick;
    if (Hit.bBlockingHit) return nullptr;
    if (GetWorld()->SweepSingleByChannel(Hit, View, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(22.f), Query))
        return Cast<ABoywickCharacter>(Hit.GetActor());
    return nullptr;
}

void AStoryFirstPersonCharacter::ServerInteractBoywick_Implementation(ABoywickCharacter* Boywick)
{
    if (!IsValid(Boywick) || FVector::Dist(Boywick->GetActorLocation(), GetActorLocation()) > 650.f) return;
    Boywick->Interact(this);
}

void AStoryFirstPersonCharacter::GrantBoywickBlessing(uint8 Kind, float Seconds)
{
    if (!HasAuthority() || Kind == 0) return;
    BoywickBlessing = Kind;
    BoywickBlessingUntil = BoywickServerNow(GetWorld()) + FMath::Max(1.f, Seconds);
    ForceNetUpdate();
    ClientHouseMaskNotice(FString::Printf(TEXT("Boywick's blessing: %s for %d seconds."),
        *ABoywickCharacter::BlessingName(EBoywickBlessing(Kind)), FMath::RoundToInt(Seconds)));
    UE_LOG(LogTemp, Display, TEXT("BOYWICK_BLESSING kind=%d seconds=%.0f player=%s"), Kind, Seconds, *GetName());
}

bool AStoryFirstPersonCharacter::HasBoywickBlessing(uint8 Kind) const
{
    return BoywickBlessing == Kind && BoywickServerNow(GetWorld()) < BoywickBlessingUntil;
}

float AStoryFirstPersonCharacter::BoywickDamageMultiplier() const
{
    return HasBoywickBlessing(uint8(EBoywickBlessing::Might)) ? 1.6f : 1.f;
}

FString AStoryFirstPersonCharacter::BoywickBlessingStatus() const
{
    const float Remaining = BoywickBlessingUntil - BoywickServerNow(GetWorld());
    if (BoywickBlessing == 0 || Remaining <= 0.f) return FString();
    return FString::Printf(TEXT("BLESSING: %s  %ds"), *ABoywickCharacter::BlessingName(EBoywickBlessing(BoywickBlessing)), FMath::CeilToInt(Remaining));
}

void AStoryFirstPersonCharacter::UpdateBoywickBlessing(float)
{
    UStoryMovementComponent* Movement = GetStoryMovement();
    if (!Movement) return;
    const uint8 Active = BoywickBlessing != 0 && BoywickServerNow(GetWorld()) < BoywickBlessingUntil ? BoywickBlessing : 0;
    if (Active == BoywickBlessingApplied) return;
    // Restore whatever the previous blessing changed, then apply the new one from the same base values.
    if (BoywickBlessingApplied != 0)
    {
        Movement->MaxWalkSpeed = BoywickBaseWalk;
        Movement->SprintSpeed = BoywickBaseSprint;
        Movement->JumpZVelocity = BoywickBaseJump;
    }
    else
    {
        BoywickBaseWalk = Movement->MaxWalkSpeed;
        BoywickBaseSprint = Movement->SprintSpeed;
        BoywickBaseJump = Movement->JumpZVelocity;
    }
    switch (EBoywickBlessing(Active))
    {
    case EBoywickBlessing::Speed:
        Movement->MaxWalkSpeed = BoywickBaseWalk * 1.4f;
        Movement->SprintSpeed = BoywickBaseSprint * 1.4f;
        break;
    case EBoywickBlessing::Jump:
        Movement->JumpZVelocity = BoywickBaseJump * 1.5f;
        break;
    default: break;
    }
    BoywickBlessingApplied = Active;
}
