#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BoywickCharacter.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class UBoxComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class APawn;
class ABoywickMetaHumanVisual;

/** What Boywick is doing right now (server authoritative, replicated for the HUD and animation). */
UENUM(BlueprintType)
enum class EBoywickMood : uint8 { Wandering, Idle, Talking, Angry, KnockedOut };

/** Rewards handed out for listening to a whole story. */
UENUM(BlueprintType)
enum class EBoywickBlessing : uint8 { None, Speed, Jump, Might };

USTRUCT(BlueprintType)
struct FBoywickStory
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Title;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> Lines;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBoywickBlessing Reward = EBoywickBlessing::Speed;
};

/**
 * Boy Wick: a short, stocky, blond MetaHuman resident of the upstairs bedroom.
 * Wanders his room, tells stories to anyone who presses E and stays to listen,
 * rewards patient listeners with a timed blessing, and turns violent when hit.
 */
UCLASS(Blueprintable)
class SENIORSENDOFF_API ABoywickCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    ABoywickCharacter();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ---- design knobs -------------------------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boywick") FString DisplayName = TEXT("Boy Wick");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Boywick|Health") float MaxHealth = 120.f;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Boywick|Health") float Health = 120.f;
    UPROPERTY(ReplicatedUsing=OnRep_Mood, BlueprintReadOnly, Category="Boywick|AI") EBoywickMood Mood = EBoywickMood::Idle;
    /** Feet-level box he wanders inside (world cm). Set by the room subsystem or in the editor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boywick|AI") FVector RoomMinimum = FVector(-2930, -1340, 290);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boywick|AI") FVector RoomMaximum = FVector(-2400, -1020, 360);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boywick|AI") float WalkSpeed = 135.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boywick|AI") float AngrySpeed = 360.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boywick|AI") float ShoveRange = 95.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boywick|AI") float ShoveStrength = 720.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boywick|AI") float ShoveCooldown = 2.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boywick|AI") float CalmDownSeconds = 22.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boywick|AI") float KnockoutSeconds = 28.f;
    /** How far and how attentively a listener must stay for the story to continue. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boywick|Stories") float ListenRange = 430.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boywick|Stories") float ListenFacing = .05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boywick|Stories") float AttentionGrace = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boywick|Stories") float BlessingSeconds = 90.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boywick|Stories") TArray<FBoywickStory> Stories;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boywick|Appearance") int32 PersonalitySeed = 1991;

    // ---- replicated story state ---------------------------------------------------------------
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Boywick|Stories") TObjectPtr<APawn> Listener;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Boywick|Stories") int32 StoryIndex = INDEX_NONE;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Boywick|Stories") int32 LineIndex = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Boywick|Stories") float LineStartedAt = 0.f;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Boywick|Stories") int32 NextStoryIndex = 0;
    /** Short one-off remark (hit reactions, dismissals); empty when none. */
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Boywick|Stories") FString Remark;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Boywick|Stories") float RemarkUntil = 0.f;

    // ---- diagnostics ----------------------------------------------------------------------------
    UPROPERTY(Transient, BlueprintReadOnly, Category="Boywick|Diagnostics") int32 StoriesTold = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Boywick|Diagnostics") int32 BlessingsGranted = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Boywick|Diagnostics") int32 Shoves = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Boywick|Diagnostics") int32 Knockouts = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Boywick|Diagnostics") int32 DamageEvents = 0;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Boywick|Diagnostics") float TravelDistance = 0.f;

    bool IsMetaHumanReady() const;
    float StoryLineElapsed() const;
    ABoywickMetaHumanVisual* GetMetaHumanVisual() const { return MetaHumanVisual; }

    // ---- queries used by the player, the HUD and tests ----------------------------------------
    UFUNCTION(BlueprintPure, Category="Boywick") FVector FeetLocation() const;
    UFUNCTION(BlueprintPure, Category="Boywick") FVector GetInteractionLocation() const;
    UFUNCTION(BlueprintPure, Category="Boywick") bool ShouldShowHealthTo(APlayerController* Viewer, FVector& LabelWorld) const;
    UFUNCTION(BlueprintPure, Category="Boywick") bool IsTelling() const { return Mood == EBoywickMood::Talking && Stories.IsValidIndex(StoryIndex); }
    UFUNCTION(BlueprintPure, Category="Boywick") bool IsTellingTo(const APawn* Pawn) const { return IsTelling() && Listener == Pawn; }
    UFUNCTION(BlueprintPure, Category="Boywick") FString CurrentLine() const;
    UFUNCTION(BlueprintPure, Category="Boywick") FString CurrentStoryTitle() const;
    UFUNCTION(BlueprintPure, Category="Boywick") float StoryProgress() const;
    UFUNCTION(BlueprintPure, Category="Boywick") FString InteractionPrompt(const APawn* For) const;
    UFUNCTION(BlueprintPure, Category="Boywick") FString CurrentRemark() const;
    /** What a listener would earn for the story he would tell next. */
    UFUNCTION(BlueprintPure, Category="Boywick") EBoywickBlessing UpcomingReward() const;
    static FString BlessingName(EBoywickBlessing Kind);

    /** Server: E pressed on him by Player. */
    bool Interact(APawn* Player);
    /** Server: force a story (tests). */
    bool BeginStory(APawn* Player, int32 Index);

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> VisualRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> WeaponHitBox;
    UPROPERTY(Transient) TObjectPtr<ABoywickMetaHumanVisual> MetaHumanVisual;
    void BuildLook();
    void Animate(float DeltaSeconds);
    UFUNCTION() void OnRep_Mood();

    // brain (server)
    void Think(float DeltaSeconds);
    void ChooseWanderGoal();
    bool FloorAt(const FVector& Feet, FVector& OutFeet) const;
    bool CanWalkStraight(const FVector& TargetFeet) const;
    void FollowGoal(float DeltaSeconds);
    void UpdateStory(float DeltaSeconds);
    void FinishStory(bool bRewarded);
    void UpdateAnger(float DeltaSeconds);
    void SetRemark(const FString& Text, float Seconds = 3.5f);
    APawn* NearestPlayer(float& OutDistance) const;
    float LineDuration(const FString& Line) const;
    float ServerNow() const;

    FRandomStream Personality;
    FVector Goal = FVector::ZeroVector;
    bool bHasGoal = false;
    float IdleUntil = 0.f, NoProgressSeconds = 0.f, BestGoalDistance = BIG_NUMBER, AttentionLapse = 0.f;
    float AngryUntil = 0.f, LastShoveAt = -100.f, KnockedOutUntil = 0.f, LastHitAt = -100.f;
    FVector LastFeet = FVector::ZeroVector;
    float LastYawForTurn = 0.f;

    float KnockoutBlend = 0.f;
};
