#include "HousePokerTable.h"
#include "StoryCampaign.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

AHousePokerTable::AHousePokerTable()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SetNetUpdateFrequency(8.f);
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);
    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("PokerInteraction"));
    InteractionBox->SetupAttachment(SceneRoot);
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBox->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    InteractionBox->SetGenerateOverlapEvents(false);
    InteractionBox->SetCanEverAffectNavigation(false);
    for (int32 i = 0; i < 5; ++i)
    {
        UStaticMeshComponent* Card = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("DealtCard%d"), i));
        Card->SetupAttachment(SceneRoot);
        Card->SetMobility(EComponentMobility::Movable);
        Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Card->SetGenerateOverlapEvents(false);
        Card->SetCanEverAffectNavigation(false);
        Card->SetCastShadow(false);
        Card->SetVisibility(false);
        CardComponents.Add(Card);
        CardDestinations.Add(FTransform(FRotator(0, (i-2)*3.f, 0), FVector((i-2)*14.f, 35, 77.12f)));
    }
}

void AHousePokerTable::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    InteractionBox->SetRelativeLocation(FocusCenter);
    InteractionBox->SetBoxExtent(FocusHalfExtent.GetAbs());
    for (UStaticMeshComponent* Card : CardComponents)
        if (Card) { Card->SetStaticMesh(CardMesh); Card->SetVisibility(false); }
}

void AHousePokerTable::BeginPlay()
{
    Super::BeginPlay();
    RefreshCardAssets();
    if (HasHand()) StartLocalDeal();
}

void AHousePokerTable::RefreshCardAssets()
{
    CardMaterials.Reset();
    for (UStaticMeshComponent* Card : CardComponents)
    {
        Card->SetStaticMesh(CardMesh);
        UMaterialInstanceDynamic* Dynamic = CardMaterial ? UMaterialInstanceDynamic::Create(CardMaterial, this) : nullptr;
        CardMaterials.Add(Dynamic);
        if (Dynamic)
        {
            const int32 FaceSlot = CardMesh ? CardMesh->GetMaterialIndex(TEXT("POK_M_CardAtlas")) : INDEX_NONE;
            Card->SetMaterial(FaceSlot == INDEX_NONE ? 0 : FaceSlot, Dynamic);
        }
        Card->SetVisibility(false);
    }
}

FVector AHousePokerTable::InteractionLocation() const
{
    return GetActorTransform().TransformPosition(FocusCenter);
}

bool AHousePokerTable::IsFocusedBy(const AStoryFirstPersonCharacter* Pawn) const
{
    if (!IsValid(Pawn) || !GetWorld() || !Pawn->FirstPersonCamera || !CardMesh || !CardMaterial) return false;
    const FVector View = Pawn->FirstPersonCamera->GetComponentLocation();
    const FVector Forward = Pawn->GetBaseAimRotation().Vector();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HousePokerFocus), false);
    Query.AddIgnoredActor(Pawn);
    FHitResult Hit;
    return GetWorld()->LineTraceSingleByChannel(Hit, View,
        View + Forward * FMath::Clamp(InteractionRange, 50.f, 200.f), ECC_Visibility, Query)
        && Hit.GetActor() == this && Hit.GetComponent() == InteractionBox;
}

float AHousePokerTable::ServerNow() const
{
    const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    return State ? State->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
}

bool AHousePokerTable::DealHand(AStoryFirstPersonCharacter* Pawn)
{
    if (!HasAuthority() || bAnimating || !IsFocusedBy(Pawn) || CardDestinations.Num() != 5) return false;
    TArray<int32> Deck;
    for (int32 i = 0; i < 52; ++i) Deck.Add(i);
    for (int32 i = 0; i < 5; ++i) Deck.Swap(i, FMath::RandRange(i, 51));
    TArray<int32> Next = {Deck[0], Deck[1], Deck[2], Deck[3], Deck[4]};
    // Ensure successive visible hands differ even in the vanishingly unlikely
    // event that the shuffle produces the same five-card set.
    TArray<int32> PriorSorted = Deal.Cards, NextSorted = Next;
    PriorSorted.Sort(); NextSorted.Sort();
    if (NextSorted == PriorSorted) Next[4] = Deck[5];
    Deal.Cards = MoveTemp(Next);
    ++Deal.Serial;
    Deal.ServerStartedAt = ServerNow();
    StartLocalDeal();
    ForceNetUpdate();
    return true;
}

void AHousePokerTable::OnRep_Deal()
{
    if (HasActorBegunPlay()) StartLocalDeal();
}

void AHousePokerTable::StartLocalDeal()
{
    if (Deal.Cards.Num() != 5 || CardDestinations.Num() != 5) return;
    if (CardMaterials.Num() != 5) RefreshCardAssets();
    ActiveSerial = Deal.Serial;
    for (int32 i = 0; i < 5; ++i)
    {
        if (CardMaterials[i]) CardMaterials[i]->SetScalarParameterValue(TEXT("CardIndex"), Deal.Cards[i]);
        CardComponents[i]->SetVisibility(false);
    }
    bAnimating = true;
    SetActorTickEnabled(true);
    AnimateCards();
}

void AHousePokerTable::AnimateCards()
{
    if (Deal.Cards.Num() != 5 || CardDestinations.Num() != 5) return;
    const float Elapsed = FMath::Max(0.f, ServerNow() - Deal.ServerStartedAt);
    bool bFinished = true;
    for (int32 i = 0; i < 5; ++i)
    {
        const float Age = Elapsed - i * FMath::Max(.05f, DealInterval);
        if (Age < 0.f) { CardComponents[i]->SetVisibility(false); bFinished = false; continue; }
        const float T = FMath::Clamp(Age / FMath::Max(.1f, SlideDuration), 0.f, 1.f);
        const float Ease = T*T*(3.f-2.f*T);
        const FTransform& Destination = CardDestinations[i];
        FVector Position = FMath::Lerp(DeckOrigin, Destination.GetLocation(), Ease);
        Position.Z += FMath::Sin(T * PI) * 1.8f;
        const FQuat Rotation = FQuat::Slerp(FQuat::Identity, Destination.GetRotation(), Ease);
        CardComponents[i]->SetRelativeTransform(FTransform(Rotation, Position, Destination.GetScale3D()));
        CardComponents[i]->SetVisibility(true);
        if (T < 1.f) bFinished = false;
    }
    if (bFinished) { bAnimating = false; SetActorTickEnabled(false); }
}

void AHousePokerTable::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AnimateCards();
}

int32 AHousePokerTable::GetVisibleCardCount() const
{
    int32 Count = 0;
    for (const UStaticMeshComponent* Card : CardComponents) if (Card && Card->IsVisible()) ++Count;
    return Count;
}

bool AHousePokerTable::AreCardsSettled() const
{
    if (bAnimating || !HasHand() || ActiveSerial != Deal.Serial || CardDestinations.Num() != 5) return false;
    for (int32 i = 0; i < 5; ++i)
        if (!CardComponents[i]->IsVisible() || !CardComponents[i]->GetRelativeTransform().Equals(CardDestinations[i], .02f)) return false;
    return true;
}

FString AHousePokerTable::HandName() const { return DescribeHand(Deal.Cards); }
FString AHousePokerTable::InteractionPrompt() const
{
    return bAnimating ? TEXT("DEALING...") : HasHand() ? TEXT("E  DEAL AGAIN") : TEXT("E  DEAL FIVE CARDS");
}

FString AHousePokerTable::DescribeHand(const TArray<int32>& Cards)
{
    if (Cards.Num() != 5) return TEXT("");
    int32 Ranks[15] = {}, Suits[4] = {};
    TSet<int32> Seen;
    for (const int32 Card : Cards)
    {
        if (Card < 0 || Card > 51 || Seen.Contains(Card)) return TEXT("");
        Seen.Add(Card);
        const int32 Rank = Card % 13 == 0 ? 14 : Card % 13 + 1;
        ++Ranks[Rank]; ++Suits[Card / 13];
    }
    int32 Pairs = 0, Trips = 0, Quads = 0;
    TArray<int32> Unique;
    for (int32 Rank = 2; Rank <= 14; ++Rank)
    {
        if (Ranks[Rank]) Unique.Add(Rank);
        if (Ranks[Rank] == 2) ++Pairs;
        if (Ranks[Rank] == 3) ++Trips;
        if (Ranks[Rank] == 4) ++Quads;
    }
    const bool bFlush = Suits[0] == 5 || Suits[1] == 5 || Suits[2] == 5 || Suits[3] == 5;
    const bool bWheel = Unique.Num() == 5 && Unique[0] == 2 && Unique[1] == 3 && Unique[2] == 4 && Unique[3] == 5 && Unique[4] == 14;
    const bool bStraight = Unique.Num() == 5 && (Unique[4] - Unique[0] == 4 || bWheel);
    if (bStraight && bFlush) return !bWheel && Unique[4] == 14 ? TEXT("ROYAL FLUSH") : TEXT("STRAIGHT FLUSH");
    if (Quads) return TEXT("FOUR OF A KIND");
    if (Trips && Pairs) return TEXT("FULL HOUSE");
    if (bFlush) return TEXT("FLUSH");
    if (bStraight) return TEXT("STRAIGHT");
    if (Trips) return TEXT("THREE OF A KIND");
    if (Pairs == 2) return TEXT("TWO PAIR");
    if (Pairs == 1) return TEXT("ONE PAIR");
    return TEXT("HIGH CARD");
}

void AHousePokerTable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AHousePokerTable, Deal);
}
