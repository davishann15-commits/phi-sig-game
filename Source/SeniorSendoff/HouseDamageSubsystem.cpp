#include "HouseDamageSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"

AHouseDamageState::AHouseDamageState()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false);
    NetUpdateFrequency = 2.f;
}

void AHouseDamageState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AHouseDamageState, TotalBrokenCount);
    DOREPLIFETIME(AHouseDamageState, CategoryCounts);
    DOREPLIFETIME(AHouseDamageState, LastBrokenCategory);
}

int32 AHouseDamageState::CountFor(FName Category) const
{
    for (const FHouseDamageCategoryCount& Entry : CategoryCounts)
        if (Entry.Category == Category) return Entry.Count;
    return 0;
}

bool UHouseDamageSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

AHouseDamageState* UHouseDamageSubsystem::FindState() const
{
    if (State.IsValid()) return State.Get();
    if (UWorld* World = GetWorld())
    {
        TActorIterator<AHouseDamageState> It(World);
        if (It) { State = *It; return *It; }
    }
    return nullptr;
}

bool UHouseDamageSubsystem::RegisterBrokenActor(AActor* BrokenActor, FName Category, AController* Instigator)
{
    UWorld* World = GetWorld();
    if (!World || !IsValid(BrokenActor) || BrokenActor->GetWorld() != World
        || !BrokenActor->HasAuthority() || World->IsNetMode(NM_Client)
        || CountedActors.Contains(BrokenActor)) return false;
    // Calder State's furniture and doors (HouseCampusInteriors) break like the house's, but they are not house damage.
    if (BrokenActor->ActorHasTag(TEXT("SSOCampusDamage"))) return false;

    AHouseDamageState* Record = FindState();
    if (!Record)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Record = World->SpawnActor<AHouseDamageState>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
        State = Record;
    }
    if (!Record) return false;

    CountedActors.Add(BrokenActor);
    if (Category.IsNone()) Category = TEXT("Furniture");
    ++Record->TotalBrokenCount;
    FHouseDamageCategoryCount* Entry = Record->CategoryCounts.FindByPredicate(
        [Category](const FHouseDamageCategoryCount& Item) { return Item.Category == Category; });
    if (!Entry)
    {
        FHouseDamageCategoryCount NewEntry;
        NewEntry.Category = Category;
        Record->CategoryCounts.Add(NewEntry);
        Entry = &Record->CategoryCounts.Last();
    }
    ++Entry->Count;
    Record->LastBrokenCategory = Category;
    Record->ForceNetUpdate();

    TotalBrokenCount = Record->TotalBrokenCount;
    BrokenFurnitureCount = Record->CountFor(TEXT("Furniture"));
    BrokenWindowCount = Record->CountFor(TEXT("Window"));
    UE_LOG(LogTemp, Display, TEXT("HOUSE_DAMAGE_COUNT actor=%s category=%s total=%d instigator=%s"),
        *BrokenActor->GetName(), *Category.ToString(), TotalBrokenCount, *GetNameSafe(Instigator));
    return true;
}

int32 UHouseDamageSubsystem::GetTotalBrokenCount() const
{
    const AHouseDamageState* Record = FindState();
    return Record ? Record->TotalBrokenCount : 0;
}

int32 UHouseDamageSubsystem::GetBrokenCount(FName Category) const
{
    const AHouseDamageState* Record = FindState();
    return Record ? Record->CountFor(Category) : 0;
}

void UHouseDamageSubsystem::AddCameraImpact(const FVector& Location, float StrengthCm)
{
    const UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_DedicatedServer || StrengthCm <= 0.f) return;
    const double Now = World->GetTimeSeconds();
    CameraImpacts.RemoveAll([Now](const FCameraImpact& Impact) { return Now - Impact.Time > .6; });
    if (CameraImpacts.Num() >= 8) CameraImpacts.RemoveAt(0);
    FCameraImpact& Impact = CameraImpacts.AddDefaulted_GetRef();
    Impact.Location = Location; Impact.Strength = StrengthCm; Impact.Time = Now;
}

FVector UHouseDamageSubsystem::SampleCameraImpact(const FVector& CameraLocation) const
{
    const UWorld* World = GetWorld();
    if (!World || CameraImpacts.IsEmpty()) return FVector::ZeroVector;
    const double Now = World->GetTimeSeconds();
    FVector Offset = FVector::ZeroVector;
    for (int32 Index = 0; Index < CameraImpacts.Num(); ++Index)
    {
        const FCameraImpact& Impact = CameraImpacts[Index];
        const float T = float(Now - Impact.Time);
        if (T < 0.f || T > .6f) continue;
        // Close breaks jolt hard, distant ones barely register.
        const float Falloff = FMath::Square(FMath::Clamp(1.f - FVector::Dist(CameraLocation, Impact.Location) / 900.f, 0.f, 1.f));
        // Fast attack, quick exponential decay.
        const float Envelope = FMath::Exp(-T * 9.f) * (1.f - FMath::Exp(-T * 60.f));
        const float Phase = Index * 1.7f;
        Offset += Impact.Strength * Falloff * Envelope
            * FVector(FMath::Sin(T * 71.f + Phase) * .6f, FMath::Sin(T * 63.f + Phase * 2.f) * .8f, FMath::Sin(T * 89.f + Phase * 3.f));
    }
    return Offset;
}
