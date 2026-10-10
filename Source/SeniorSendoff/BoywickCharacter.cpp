#include "BoywickCharacter.h"
#include "BoywickMetaHumanVisual.h"
#include "StoryCampaign.h"
#include "HouseBlackHallPaint.h"
#include "HouseBlackHallLook.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"
#include <initializer_list>

namespace
{
const TCHAR* AngryRemarks[] = {
    TEXT("OW. Okay. OKAY."),
    TEXT("You hit the HAIR."),
    TEXT("Big mistake, buddy."),
    TEXT("That's it. That is IT."),
    TEXT("I was being NICE to you!"),
};
}

ABoywickCharacter::ABoywickCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
    bUseControllerRotationYaw = false;
    GetCapsuleComponent()->SetCapsuleSize(36.f, 83.f);
    if (USkeletalMeshComponent* Skel = GetMesh()) { Skel->SetVisibility(false); Skel->SetCollisionEnabled(ECollisionEnabled::NoCollision); }

    AutoPossessAI = EAutoPossessAI::Disabled;
    SetNetUpdateFrequency(20.f);
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    // No AI controller: the brain below feeds AddMovementInput directly, so let the movement component run without one.
    Movement->bRunPhysicsWithNoController = true;
    Movement->MaxStepHeight = 24.f;
    Movement->MaxWalkSpeed = WalkSpeed;
    Movement->bOrientRotationToMovement = true;
    Movement->RotationRate = FRotator(0.f, 460.f, 0.f);
    Movement->MaxAcceleration = 1100.f;
    Movement->BrakingDecelerationWalking = 1400.f;
    Movement->SetWalkableFloorAngle(46.f);
    Movement->bCanWalkOffLedges = false;
    Movement->NavAgentProps.bCanCrouch = false;

    // The independent native MetaHuman is attached at the feet. The capsule
    // remains authoritative for movement, interaction and replicated AI.
    VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BoywickVisualRoot"));
    VisualRoot->SetupAttachment(GetCapsuleComponent());
    VisualRoot->SetRelativeLocation(FVector(0, 0, -83.f));

    // The thrown hat and sauce sweep the visibility channel; give them something to hit.
    WeaponHitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoywickWeaponHitBox"));
    WeaponHitBox->SetupAttachment(VisualRoot);
    WeaponHitBox->SetRelativeLocation(FVector(4.f, 0, 83.f));
    WeaponHitBox->SetBoxExtent(FVector(31.f, 36.f, 81.f));
    WeaponHitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    WeaponHitBox->SetCollisionObjectType(ECC_WorldDynamic);
    WeaponHitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    WeaponHitBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    WeaponHitBox->SetGenerateOverlapEvents(false);
    SetCanBeDamaged(true);

    auto Story = [this](const TCHAR* Title, EBoywickBlessing Reward, std::initializer_list<const TCHAR*> Lines)
    {
        FBoywickStory S; S.Title = Title; S.Reward = Reward;
        for (const TCHAR* L : Lines) S.Lines.Add(L);
        Stories.Add(MoveTemp(S));
    };
    Story(TEXT("The Night the Pigs Got Loose"), EBoywickBlessing::Speed, {
        TEXT("You met Pepper and Coal? Sweetest animals in this house."),
        TEXT("One night the hall door clicked open and nobody saw a thing."),
        TEXT("By two a.m. there were hoofprints in the chapter room and Cheeto dust on the stairs."),
        TEXT("I tracked them with a flashlight and half a sandwich."),
        TEXT("Found them asleep on the library rug like they'd paid dues."),
        TEXT("Now they live in the closet down there. Keep that door SHUT."),
    });
    Story(TEXT("Why They Call Me Boywick"), EBoywickBlessing::Jump, {
        TEXT("Freshman year I showed up with a bowl cut and a cooler of ice cream sandwiches."),
        TEXT("A senior looked at my hair and said, 'That is a perfect scoop.'"),
        TEXT("The name Boywick stuck. The ice cream was gone in nine minutes."),
        TEXT("Every scoop since has been a tribute. Don't touch the hair."),
        TEXT("Seriously. Don't."),
    });
    Story(TEXT("The Senior Sendoff"), EBoywickBlessing::Might, {
        TEXT("Every spring the seniors stop acting like people."),
        TEXT("They wander. They moan about 'the real world.' They eat everything."),
        TEXT("One of them carried the couch to the porch and just sat there for a day."),
        TEXT("That's why we lock the good furniture and hide the Cheetos."),
        TEXT("A wave like that comes through, you hold the house. Simple as that."),
    });
    Story(TEXT("The Vent"), EBoywickBlessing::Speed, {
        TEXT("There's a vent in my closet. Don't look at it like that, it's fine."),
        TEXT("A pledge dared me to crawl it. I got as far as the library ceiling."),
        TEXT("Heard the pigs snoring right under me. Peaceful, honestly."),
        TEXT("Then I got stuck for forty minutes and ate a granola bar I found in there."),
        TEXT("Zero regrets. Nine out of ten. Would crawl again."),
    });
    Story(TEXT("The Legend of the Flavor Banner"), EBoywickBlessing::Jump, {
        TEXT("That banner is older than anyone in this house."),
        TEXT("Story goes it blew off a food truck at homecoming and a brother caught it mid-air."),
        TEXT("We hung it up and won the chili cook-off three years straight."),
        TEXT("Then somebody WASHED it, and we lost. Never wash the banner."),
    });
    Story(TEXT("The Loft Bed"), EBoywickBlessing::Might, {
        TEXT("Built that loft myself. Four trips to the hardware store, two to the ER."),
        TEXT("Kidding about the ER. One trip."),
        TEXT("Slept up there through a tornado warning and a fire alarm."),
        TEXT("If it ever squeaks, that's just the house saying hi."),
    });
}

void ABoywickCharacter::BuildLook()
{
    if (GetNetMode() == NM_DedicatedServer || MetaHumanVisual) return;
    FActorSpawnParameters Params;
    Params.Owner = this; Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    MetaHumanVisual = GetWorld()->SpawnActor<ABoywickMetaHumanVisual>(ABoywickMetaHumanVisual::StaticClass(), VisualRoot->GetComponentTransform(), Params);
    if (MetaHumanVisual)
    {
        MetaHumanVisual->AttachToComponent(VisualRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        if (!MetaHumanVisual->Initialize(this))
            UE_LOG(LogTemp, Error, TEXT("BOYWICK_METAHUMAN_FAILED native character or animations missing"));
    }
}

bool ABoywickCharacter::IsMetaHumanReady() const
{
    return MetaHumanVisual && MetaHumanVisual->IsReady();
}

float ABoywickCharacter::StoryLineElapsed() const
{
    return IsTelling() ? FMath::Max(0.f, ServerNow() - LineStartedAt) : 0.f;
}

void ABoywickCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    if (IsValid(MetaHumanVisual)) MetaHumanVisual->Destroy();
    MetaHumanVisual = nullptr;
    Super::EndPlay(Reason);
}

void ABoywickCharacter::BeginPlay()
{
    Super::BeginPlay();
    BuildLook();
    Personality.Initialize(PersonalitySeed);
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    if (HasAuthority())
    {
        Health = FMath::Max(1.f, MaxHealth);
        Mood = EBoywickMood::Idle;
        IdleUntil = ServerNow() + Personality.FRandRange(1.f, 3.f);
    }
    LastFeet = FeetLocation();
    if (FParse::Param(FCommandLine::Get(), TEXT("BoywickReview"))) SetActorRotation(FRotator::ZeroRotator);
    UE_LOG(LogTemp, Display, TEXT("BOYWICK_BEGIN feet=%s room=[%s..%s] stories=%d authority=%d"),
        *FeetLocation().ToString(), *RoomMinimum.ToString(), *RoomMaximum.ToString(), Stories.Num(), HasAuthority() ? 1 : 0);
}

void ABoywickCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (HasAuthority() && !FParse::Param(FCommandLine::Get(), TEXT("BoywickReview"))) Think(DeltaSeconds);
    Animate(DeltaSeconds);
}

float ABoywickCharacter::ServerNow() const
{
    if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr) return GS->GetServerWorldTimeSeconds();
    return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

FVector ABoywickCharacter::FeetLocation() const
{
    return GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

FVector ABoywickCharacter::GetInteractionLocation() const
{
    return FeetLocation() + FVector(0, 0, Mood == EBoywickMood::KnockedOut ? 30.f : 142.f);
}

bool ABoywickCharacter::ShouldShowHealthTo(APlayerController* Viewer, FVector& LabelWorld) const
{
    if (!Viewer || !GetWorld()) return false;
    FVector View; FRotator Rotation; Viewer->GetPlayerViewPoint(View, Rotation);
    LabelWorld = FeetLocation() + FVector(0, 0, Mood == EBoywickMood::KnockedOut ? 70.f : 184.f);
    const FVector To = LabelWorld - View;
    if (To.SizeSquared() > FMath::Square(1300.f) || FVector::DotProduct(To.GetSafeNormal(), Rotation.Vector()) < .3f) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BoywickHealthVisibility), false, this);
    Query.AddIgnoredActor(Viewer->GetPawn());
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit, View, GetInteractionLocation(), ECC_Visibility, Query);
}

FString ABoywickCharacter::CurrentLine() const
{
    if (!IsTelling() || !Stories[StoryIndex].Lines.IsValidIndex(LineIndex)) return FString();
    return Stories[StoryIndex].Lines[LineIndex];
}

FString ABoywickCharacter::CurrentStoryTitle() const
{
    return IsTelling() ? Stories[StoryIndex].Title : FString();
}

float ABoywickCharacter::StoryProgress() const
{
    if (!IsTelling() || Stories[StoryIndex].Lines.Num() == 0) return 0.f;
    const FBoywickStory& Story = Stories[StoryIndex];
    const float Within = Story.Lines.IsValidIndex(LineIndex)
        ? FMath::Clamp((ServerNow() - LineStartedAt) / LineDuration(Story.Lines[LineIndex]), 0.f, 1.f) : 0.f;
    return FMath::Clamp((LineIndex + Within) / Story.Lines.Num(), 0.f, 1.f);
}

FString ABoywickCharacter::CurrentRemark() const
{
    return ServerNow() < RemarkUntil ? Remark : FString();
}

EBoywickBlessing ABoywickCharacter::UpcomingReward() const
{
    return Stories.IsValidIndex(NextStoryIndex) ? Stories[NextStoryIndex].Reward : EBoywickBlessing::None;
}

FString ABoywickCharacter::BlessingName(EBoywickBlessing Kind)
{
    switch (Kind)
    {
    case EBoywickBlessing::Speed: return TEXT("SPEED");
    case EBoywickBlessing::Jump: return TEXT("JUMP");
    case EBoywickBlessing::Might: return TEXT("MIGHT");
    default: return TEXT("NOTHING");
    }
}

FString ABoywickCharacter::InteractionPrompt(const APawn* For) const
{
    switch (Mood)
    {
    case EBoywickMood::KnockedOut: return TEXT("BOYWICK IS OUT COLD");
    case EBoywickMood::Angry: return TEXT("BOYWICK IS FURIOUS");
    case EBoywickMood::Talking: return Listener == For ? TEXT("LISTENING...  (STAY CLOSE)") : TEXT("BOYWICK IS BUSY");
    default: return FString::Printf(TEXT("E  LISTEN TO BOYWICK  |  %s"), *BlessingName(UpcomingReward()));
    }
}

float ABoywickCharacter::LineDuration(const FString& Line) const
{
    return 1.7f + .052f * Line.Len();
}

void ABoywickCharacter::SetRemark(const FString& Text, float Seconds)
{
    Remark = Text; RemarkUntil = ServerNow() + Seconds; ForceNetUpdate();
}

APawn* ABoywickCharacter::NearestPlayer(float& OutDistance) const
{
    APawn* Best = nullptr; OutDistance = BIG_NUMBER;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* PC = It->Get();
        if (!PC || !PC->GetPawn()) continue;
        const float D = FVector::Dist(PC->GetPawn()->GetActorLocation(), GetActorLocation());
        if (D < OutDistance) { OutDistance = D; Best = PC->GetPawn(); }
    }
    return Best;
}

// ------------------------------------------------------------------------------------------------
// Interaction and stories (server)
// ------------------------------------------------------------------------------------------------
bool ABoywickCharacter::Interact(APawn* Player)
{
    if (!HasAuthority() || !IsValid(Player)) return false;
    switch (Mood)
    {
    case EBoywickMood::KnockedOut: SetRemark(TEXT("...five more minutes...")); return false;
    case EBoywickMood::Angry: SetRemark(TEXT("NOT. NOW.")); return false;
    case EBoywickMood::Talking:
        if (Listener == Player) SetRemark(TEXT("Patience. I'm getting to the good part."), 2.5f);
        else SetRemark(TEXT("One at a time, friend."), 2.5f);
        return Listener == Player;
    default: break;
    }
    return BeginStory(Player, NextStoryIndex);
}

bool ABoywickCharacter::BeginStory(APawn* Player, int32 Index)
{
    if (!HasAuthority() || !IsValid(Player) || Stories.Num() == 0) return false;
    if (!Stories.IsValidIndex(Index)) Index = 0;
    if (Stories[Index].Lines.Num() == 0) return false;
    Listener = Player; StoryIndex = Index; LineIndex = 0; LineStartedAt = ServerNow();
    Mood = EBoywickMood::Talking; bHasGoal = false; AttentionLapse = 0.f;
    GetCharacterMovement()->StopMovementImmediately();
    Remark.Empty(); RemarkUntil = 0.f;
    ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("BOYWICK_STORY_BEGIN index=%d title=%s listener=%s"), Index, *Stories[Index].Title, *GetNameSafe(Player));
    return true;
}

void ABoywickCharacter::UpdateStory(float DeltaSeconds)
{
    const float Now = ServerNow();
    if (!IsValid(Listener) || !Stories.IsValidIndex(StoryIndex)) { FinishStory(false); return; }
    const FVector To = Listener->GetActorLocation() - GetActorLocation();
    SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, To.Rotation().Yaw, 0.f), DeltaSeconds, 7.f));
    bool bAttentive = To.Size2D() <= ListenRange && FMath::Abs(To.Z) < 200.f;
    if (bAttentive)
    {
        FVector View; FRotator Rotation; Listener->GetActorEyesViewPoint(View, Rotation);
        bAttentive = FVector::DotProduct(Rotation.Vector(), (GetInteractionLocation() - View).GetSafeNormal()) >= ListenFacing;
        if (bAttentive)
        {
            FCollisionQueryParams Query(SCENE_QUERY_STAT(BoywickStoryVisibility), false, this);
            Query.AddIgnoredActor(Listener);
            FHitResult Hit;
            bAttentive = !GetWorld()->LineTraceSingleByChannel(Hit, View, GetInteractionLocation(), ECC_Visibility, Query);
        }
    }
    AttentionLapse = bAttentive ? 0.f : AttentionLapse + DeltaSeconds;
    if (AttentionLapse > AttentionGrace)
    {
        UE_LOG(LogTemp, Display, TEXT("BOYWICK_STORY_ABANDONED index=%d line=%d"), StoryIndex, LineIndex);
        FinishStory(false);
        SetRemark(TEXT("Hey! I wasn't finished!"), 4.f);
        return;
    }
    const FBoywickStory& Story = Stories[StoryIndex];
    if (!Story.Lines.IsValidIndex(LineIndex)) { FinishStory(true); return; }
    if (Now - LineStartedAt >= LineDuration(Story.Lines[LineIndex]))
    {
        ++LineIndex; LineStartedAt = Now; ForceNetUpdate();
        if (LineIndex >= Story.Lines.Num()) FinishStory(true);
    }
}

void ABoywickCharacter::FinishStory(bool bRewarded)
{
    const float Now = ServerNow();
    if (bRewarded && Stories.IsValidIndex(StoryIndex))
    {
        const FBoywickStory& Story = Stories[StoryIndex];
        ++StoriesTold;
        if (AStoryFirstPersonCharacter* Player = Cast<AStoryFirstPersonCharacter>(Listener.Get()))
        {
            Player->GrantBoywickBlessing(uint8(Story.Reward), BlessingSeconds);
            ++BlessingsGranted;
        }
        NextStoryIndex = (StoryIndex + 1) % FMath::Max(1, Stories.Num());
        SetRemark(FString::Printf(TEXT("That's the story. You've earned some %s."), *BlessingName(Story.Reward)), 5.f);
        UE_LOG(LogTemp, Display, TEXT("BOYWICK_STORY_DONE index=%d reward=%s listener=%s"), StoryIndex, *BlessingName(Story.Reward), *GetNameSafe(Listener));
    }
    Listener = nullptr; StoryIndex = INDEX_NONE; LineIndex = 0; LineStartedAt = 0.f; AttentionLapse = 0.f;
    if (Mood == EBoywickMood::Talking) Mood = EBoywickMood::Idle;
    IdleUntil = Now + 2.f;
    ForceNetUpdate();
}

// ------------------------------------------------------------------------------------------------
// Damage and anger (server)
// ------------------------------------------------------------------------------------------------
float ABoywickCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || Mood == EBoywickMood::KnockedOut || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f) return 0.f;
    const float Actual = FMath::Min(Health, Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser));
    Health = FMath::Max(0.f, Health - Actual);
    ++DamageEvents;
    const float Now = ServerNow();
    LastHitAt = Now;
    const bool bWasTalking = Mood == EBoywickMood::Talking;
    if (bWasTalking) FinishStory(false);
    if (Health <= 0.f)
    {
        Mood = EBoywickMood::KnockedOut; KnockedOutUntil = Now + KnockoutSeconds; ++Knockouts;
        bHasGoal = false;
        GetCharacterMovement()->StopMovementImmediately();
        SetRemark(TEXT("...tell my ice cream I loved it..."), 6.f);
        UE_LOG(LogTemp, Display, TEXT("BOYWICK_KNOCKED_OUT by=%s"), *GetNameSafe(DamageCauser));
    }
    else
    {
        Mood = EBoywickMood::Angry; AngryUntil = Now + CalmDownSeconds; bHasGoal = false;
        GetCharacterMovement()->MaxWalkSpeed = AngrySpeed;
        SetRemark(bWasTalking ? TEXT("You hit me mid-STORY?!") : AngryRemarks[Personality.RandRange(0, UE_ARRAY_COUNT(AngryRemarks) - 1)], 3.5f);
        UE_LOG(LogTemp, Display, TEXT("BOYWICK_ANGRY health=%.0f by=%s"), Health, *GetNameSafe(DamageCauser));
    }
    ForceNetUpdate();
    return Actual;
}

void ABoywickCharacter::UpdateAnger(float DeltaSeconds)
{
    const float Now = ServerNow();
    float Distance = BIG_NUMBER;
    APawn* Target = NearestPlayer(Distance);
    if (!Target || Now >= AngryUntil || Distance > 1500.f)
    {
        Mood = EBoywickMood::Idle; IdleUntil = Now + 1.5f;
        GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
        SetRemark(TEXT("We're good. We're good. Don't do that again."), 4.f);
        ForceNetUpdate();
        return;
    }
    const FVector Feet = FeetLocation();
    const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
    const FVector Dir = ToTarget.GetSafeNormal2D();
    // Shove when close enough; otherwise close in, sidestepping when stuck.
    if (ToTarget.Size2D() <= ShoveRange && FMath::Abs(ToTarget.Z) < 160.f)
    {
        if (Now - LastShoveAt >= ShoveCooldown)
        {
            LastShoveAt = Now; ++Shoves;
            if (ACharacter* Victim = Cast<ACharacter>(Target))
                Victim->LaunchCharacter(Dir * ShoveStrength + FVector(0, 0, 300.f), true, true);
            SetRemark(Personality.FRand() < .5f ? TEXT("GET OUT OF MY ROOM!") : TEXT("Say sorry to the HAIR!"), 2.5f);
            UE_LOG(LogTemp, Display, TEXT("BOYWICK_SHOVE target=%s"), *GetNameSafe(Target));
        }
        SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
        return;
    }
    const float Progress = FVector::Dist2D(Feet, LastFeet);
    NoProgressSeconds = Progress < DeltaSeconds * 25.f ? NoProgressSeconds + DeltaSeconds : 0.f;
    FVector Move = Dir;
    if (NoProgressSeconds > .8f)
    {
        const float Side = FMath::Fmod(Now, 2.4f) < 1.2f ? 1.f : -1.f;
        Move = (Dir + FVector(-Dir.Y, Dir.X, 0) * Side * 1.2f).GetSafeNormal2D();
        if (NoProgressSeconds > 2.4f) NoProgressSeconds = 0.f;
    }
    AddMovementInput(Move, 1.f);
}

// ------------------------------------------------------------------------------------------------
// Wandering (server)
// ------------------------------------------------------------------------------------------------
bool ABoywickCharacter::FloorAt(const FVector& Feet, FVector& OutFeet) const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BoywickFloor), false, this);
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Feet + FVector(0, 0, 90.f), Feet - FVector(0, 0, 90.f), ECC_Visibility, Query)) return false;
    if (Hit.ImpactNormal.Z < .9f) return false;
    if (Cast<APawn>(Hit.GetActor())) return false;
    OutFeet = Hit.ImpactPoint;
    return true;
}

bool ABoywickCharacter::CanWalkStraight(const FVector& TargetFeet) const
{
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius() * .9f;
    const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * .85f;
    const FVector Start = FeetLocation() + FVector(0, 0, Half + 8.f);
    const FVector End = TargetFeet + FVector(0, 0, Half + 8.f);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BoywickWalk), false, this);
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (const APlayerController* PC = It->Get(); PC && PC->GetPawn()) Query.AddIgnoredActor(PC->GetPawn());
    FHitResult Hit;
    return !GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, Half), Query);
}

void ABoywickCharacter::ChooseWanderGoal()
{
    const float Margin = GetCapsuleComponent()->GetScaledCapsuleRadius() + 14.f;
    for (int32 Attempt = 0; Attempt < 14; ++Attempt)
    {
        const FVector Candidate(Personality.FRandRange(RoomMinimum.X + Margin, RoomMaximum.X - Margin),
            Personality.FRandRange(RoomMinimum.Y + Margin, RoomMaximum.Y - Margin), FeetLocation().Z);
        if (FVector::Dist2D(Candidate, FeetLocation()) < 70.f) continue;
        FVector Floor;
        if (!FloorAt(Candidate, Floor) || Floor.Z < RoomMinimum.Z || Floor.Z > RoomMaximum.Z) continue;
        if (!CanWalkStraight(Floor)) continue;
        Goal = Floor; bHasGoal = true; Mood = EBoywickMood::Wandering;
        BestGoalDistance = BIG_NUMBER; NoProgressSeconds = 0.f;
        GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
        ForceNetUpdate();
        return;
    }
    Mood = EBoywickMood::Idle; IdleUntil = ServerNow() + Personality.FRandRange(1.5f, 3.f);
}

void ABoywickCharacter::FollowGoal(float DeltaSeconds)
{
    const FVector Feet = FeetLocation();
    const float Distance = FVector::Dist2D(Feet, Goal);
    if (Distance < 16.f)
    {
        bHasGoal = false; Mood = EBoywickMood::Idle;
        IdleUntil = ServerNow() + Personality.FRandRange(1.2f, 4.5f);
        GetCharacterMovement()->StopMovementImmediately();
        ForceNetUpdate();
        return;
    }
    if (Distance < BestGoalDistance - 4.f) { BestGoalDistance = Distance; NoProgressSeconds = 0.f; }
    else NoProgressSeconds += DeltaSeconds;
    if (NoProgressSeconds > 2.2f)
    {
        bHasGoal = false; Mood = EBoywickMood::Idle; IdleUntil = ServerNow() + 1.f;
        GetCharacterMovement()->StopMovementImmediately();
        return;
    }
    AddMovementInput((Goal - Feet).GetSafeNormal2D(), 1.f);
}

void ABoywickCharacter::Think(float DeltaSeconds)
{
    const float Now = ServerNow();
    const FVector Feet = FeetLocation();
    switch (Mood)
    {
    case EBoywickMood::KnockedOut:
        if (Now >= KnockedOutUntil)
        {
            Health = FMath::Max(1.f, MaxHealth * .6f);
            Mood = EBoywickMood::Idle; IdleUntil = Now + 2.f;
            GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
            SetRemark(TEXT("...what year is it?"), 4.f);
            ForceNetUpdate();
        }
        break;
    case EBoywickMood::Angry: UpdateAnger(DeltaSeconds); break;
    case EBoywickMood::Talking: UpdateStory(DeltaSeconds); break;
    default:
        if (bHasGoal) FollowGoal(DeltaSeconds);
        else if (Now >= IdleUntil)
        {
            if (Personality.FRand() < .72f) ChooseWanderGoal();
            else { Mood = EBoywickMood::Idle; IdleUntil = Now + Personality.FRandRange(2.f, 5.f); }
        }
        break;
    }
    TravelDistance += FVector::Dist2D(Feet, LastFeet);
    LastFeet = Feet;
}

// ------------------------------------------------------------------------------------------------
// Animation (every machine, cosmetic)
// ------------------------------------------------------------------------------------------------
void ABoywickCharacter::OnRep_Mood()
{
    GetCharacterMovement()->MaxWalkSpeed = Mood == EBoywickMood::Angry ? AngrySpeed : WalkSpeed;
}

void ABoywickCharacter::Animate(float DeltaSeconds)
{
    KnockoutBlend = FMath::FInterpTo(KnockoutBlend, Mood == EBoywickMood::KnockedOut ? 1.f : 0.f, DeltaSeconds, 3.f);
    if (VisualRoot)
    {
        VisualRoot->SetRelativeRotation(FRotator(0.f, 0.f, 86.f * KnockoutBlend));
        VisualRoot->SetRelativeLocation(FVector(0, -6.f * KnockoutBlend,
            -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() + 32.f * KnockoutBlend));
    }
}

void ABoywickCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABoywickCharacter, Health);
    DOREPLIFETIME(ABoywickCharacter, MaxHealth);
    DOREPLIFETIME(ABoywickCharacter, Mood);
    DOREPLIFETIME(ABoywickCharacter, Listener);
    DOREPLIFETIME(ABoywickCharacter, StoryIndex);
    DOREPLIFETIME(ABoywickCharacter, LineIndex);
    DOREPLIFETIME(ABoywickCharacter, LineStartedAt);
    DOREPLIFETIME(ABoywickCharacter, NextStoryIndex);
    DOREPLIFETIME(ABoywickCharacter, Remark);
    DOREPLIFETIME(ABoywickCharacter, RemarkUntil);
}
