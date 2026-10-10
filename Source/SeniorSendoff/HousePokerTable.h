#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HousePokerTable.generated.h"

class AStoryFirstPersonCharacter;
class UBoxComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// One replicated update carries a whole deal, so clients cannot see a new
// serial paired with the previous hand or its start time.
USTRUCT()
struct FHousePokerDeal
{
    GENERATED_BODY()
    UPROPERTY() TArray<int32> Cards;
    UPROPERTY() int32 Serial = 0;
    UPROPERTY() float ServerStartedAt = 0.f;
};

UCLASS(Blueprintable)
class SENIORSENDOFF_API AHousePokerTable : public AActor
{
    GENERATED_BODY()
public:
    AHousePokerTable();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Poker") TObjectPtr<UBoxComponent> InteractionBox;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Poker|Cards") TObjectPtr<UStaticMesh> CardMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Poker|Cards") TObjectPtr<UMaterialInterface> CardMaterial;
    // Local poses share the table's floor origin and scale. Cards are centered
    // flat meshes with upward faces. Only the face material is replaced.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Poker|Cards") FVector DeckOrigin = FVector(-63, -12, 79.4);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Poker|Cards") TArray<FTransform> CardDestinations;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Poker|Cards", meta=(ClampMin="0.05", ClampMax="1")) float DealInterval = .2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Poker|Cards", meta=(ClampMin="0.1", ClampMax="1")) float SlideDuration = .3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Poker|Interaction", meta=(ClampMin="50", ClampMax="200")) float InteractionRange = 200.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Poker|Interaction") FVector FocusCenter = FVector(0, 0, 79);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Poker|Interaction") FVector FocusHalfExtent = FVector(145, 70, 3);

    bool IsFocusedBy(const AStoryFirstPersonCharacter* Pawn) const;
    bool DealHand(AStoryFirstPersonCharacter* Pawn);
    FVector InteractionLocation() const;
    UFUNCTION(BlueprintPure, Category="Poker") bool IsDealing() const { return bAnimating; }
    UFUNCTION(BlueprintPure, Category="Poker") bool HasHand() const { return Deal.Cards.Num() == 5; }
    UFUNCTION(BlueprintPure, Category="Poker") FString HandName() const;
    UFUNCTION(BlueprintPure, Category="Poker") FString InteractionPrompt() const;
    UFUNCTION(BlueprintPure, Category="Poker") bool AreCardsSettled() const;
    UFUNCTION(BlueprintPure, Category="Poker") TArray<int32> GetHandIndices() const { return Deal.Cards; }
    UFUNCTION(BlueprintPure, Category="Poker") int32 GetDealSerial() const { return Deal.Serial; }
    UFUNCTION(BlueprintPure, Category="Poker") int32 GetVisibleCardCount() const;
    static FString DescribeHand(const TArray<int32>& Cards);

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> CardComponents;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> CardMaterials;
    UPROPERTY(ReplicatedUsing=OnRep_Deal) FHousePokerDeal Deal;
    UFUNCTION() void OnRep_Deal();
    void RefreshCardAssets();
    void StartLocalDeal();
    void AnimateCards();
    float ServerNow() const;
    bool bAnimating = false;
    int32 ActiveSerial = 0;
};
