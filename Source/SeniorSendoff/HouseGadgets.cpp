// Wall gadgets: alarm pull stations, horn/strobes, light switches and the singing bass.
#include "HouseGadgets.h"
#include "HouseDetail.h"
#include "HouseSfx.h"
#include "HouseBillyBass.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"

namespace
{
/** Mono 16 kHz horn: NFPA temporal-three pattern (three half-second blasts, 1.5 s rest) with an electronic warble. */
const TArray<int16>& HgHornSamples()
{
    static TArray<int16> Samples;
    if (Samples.Num()) return Samples;
    const int32 Rate = 16000;
    Samples.SetNumUninitialized(Rate * 25);
    for (int32 I = 0; I < Samples.Num(); ++I)
    {
        const float T = I / float(Rate);
        const float Cycle = FMath::Fmod(T, 4.f);
        float Gate = 0.f;
        if (Cycle < 3.f)
        {
            const float Ph = FMath::Fmod(Cycle, 1.f);
            if (Ph < .5f) Gate = FMath::Min(1.f, FMath::Min(Ph, .5f - Ph) / .006f);
        }
        const float Warble = FMath::Sin(2 * PI * 22.f * T);
        const float Tone = FMath::Sin(2 * PI * 2550.f * T + 1.6f * Warble) * .7f + FMath::Sin(2 * PI * 1275.f * T) * .3f;
        const float Shaped = FMath::Clamp(Tone * 2.2f, -1.f, 1.f);
        Samples[I] = int16(FMath::Clamp(Shaped * Gate * .55f, -1.f, 1.f) * 32767.f);
    }
    return Samples;
}
}

AHouseGadget::AHouseGadget()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
    Root->SetMobility(EComponentMobility::Movable);
    Focus = CreateDefaultSubobject<UBoxComponent>(TEXT("Focus"));
    Focus->SetupAttachment(Root);
    Focus->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Focus->SetCollisionObjectType(ECC_WorldStatic);
    Focus->SetCollisionResponseToAllChannels(ECR_Ignore);
    Focus->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Focus->SetGenerateOverlapEvents(false);
    Focus->SetCanEverAffectNavigation(false);
    auto MakeMesh = [this](const TCHAR* Name, USceneComponent* Parent)
    {
        UProceduralMeshComponent* PM = CreateDefaultSubobject<UProceduralMeshComponent>(Name);
        PM->SetupAttachment(Parent);
        PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PM->bUseAsyncCooking = false;
        PM->SetCastShadow(false);
        PM->SetCanEverAffectNavigation(false);
        return PM;
    };
    Fixed = MakeMesh(TEXT("Fixed"), Root);
    Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
    Hinge->SetupAttachment(Root);
    Moving = MakeMesh(TEXT("Moving"), Hinge);
    Hinge2 = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge2"));
    Hinge2->SetupAttachment(Moving);      // rides on the moving part (the bass's tail swings with its body)
    Moving2 = MakeMesh(TEXT("Moving2"), Hinge2);
    Hinge3 = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge3"));
    Hinge3->SetupAttachment(Moving);      // the bass's jaw
    Moving3 = MakeMesh(TEXT("Moving3"), Hinge3);
}

void AHouseGadget::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AHouseGadget, Kind);
    DOREPLIFETIME(AHouseGadget, Variant);
    DOREPLIFETIME(AHouseGadget, State);
    DOREPLIFETIME(AHouseGadget, StateTime);
}

void AHouseGadget::BeginPlay()
{
    Super::BeginPlay();
    BuildParts();
    if (State) ApplyState(false);
}

void AHouseGadget::OnRep_Kind() { BuildParts(); }
void AHouseGadget::OnRep_State() { ApplyState(); }

float AHouseGadget::ServerNow() const
{
    const UWorld* World = GetWorld();
    if (!World) return 0.f;
    if (const AGameStateBase* GS = World->GetGameState()) return GS->GetServerWorldTimeSeconds();
    return World->GetTimeSeconds();
}

void AHouseGadget::SetState(uint8 NewState)
{
    State = NewState;
    StateTime = ServerNow();
    ApplyState();
}

void AHouseGadget::SetAlarm(bool bOn)
{
    if (!HasAuthority()) return;
    if (Kind == EHouseGadgetKind::HornStrobe) SetState(bOn ? 1 : 0);
    else if (Kind == EHouseGadgetKind::PullStation && !bOn) SetState(0);
}

bool AHouseGadget::Interact(APawn*)
{
    if (!HasAuthority() || !GetWorld()) return false;
    switch (Kind)
    {
    case EHouseGadgetKind::PullStation:
        if (State) return false;
        SetState(1);
        if (UHouseDetailSubsystem* Detail = GetWorld()->GetSubsystem<UHouseDetailSubsystem>()) Detail->TriggerFireAlarm(24.f);
        return true;
    case EHouseGadgetKind::LightSwitch:
        SetState(State ? 0 : 1);
        return true;
    case EHouseGadgetKind::BillyBass:
        if (State) return false;
        SetState(1);
        GetWorldTimerManager().SetTimer(ResetTimer, FTimerDelegate::CreateWeakLambda(this, [this] { SetState(0); }), 5.2f, false);
        return true;
    default:
        return false;
    }
}

FString AHouseGadget::FocusLabel() const
{
    switch (Kind)
    {
    case EHouseGadgetKind::PullStation: return State ? TEXT("FIRE ALARM PULLED") : TEXT("E  PULL FIRE ALARM");
    case EHouseGadgetKind::LightSwitch: return State ? TEXT("E  TURN THE LIGHTS ON") : TEXT("E  TURN THE LIGHTS OFF");
    case EHouseGadgetKind::BillyBass: return State ? TEXT("THE BASS IS SINGING") : TEXT("E  PRESS THE BASS");
    default: return FString();
    }
}

void AHouseGadget::FindLights()
{
    if (bLightsFound || !GetWorld()) return;
    bLightsFound = true;
    const FVector Here = GetActorLocation();
    struct FCandidate { ULightComponent* Light; float Dist; };
    TArray<FCandidate> Candidates;
    for (TObjectIterator<ULightComponent> It; It; ++It)
    {
        ULightComponent* L = *It;
        if (!IsValid(L) || L->GetWorld() != GetWorld() || !L->IsRegistered() || L->IsA<UDirectionalLightComponent>()) continue;
        const FVector D = L->GetComponentLocation() - Here;
        if (FMath::Abs(D.Z) > 260.f) continue;
        const float Dist = D.Size2D();
        if (Dist > 480.f) continue;
        Candidates.Add({ L, Dist });
    }
    Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Dist < B.Dist; });
    for (int32 I = 0; I < FMath::Min(4, Candidates.Num()); ++I) Lights.Add(Candidates[I].Light);
}

void AHouseGadget::StartHorn()
{
    if (Horn || GetNetMode() == NM_DedicatedServer || !GetWorld()) return;
    USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
    Wave->SetSampleRate(16000);
    Wave->NumChannels = 1;
    Wave->Duration = INDEFINITELY_LOOPING_DURATION;
    Wave->SoundGroup = SOUNDGROUP_Default;
    Wave->bLooping = false;
    const TArray<int16>& Pcm = HgHornSamples();
    Wave->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
    USoundAttenuation* Att = NewObject<USoundAttenuation>(this);
    Att->Attenuation.bAttenuate = true;
    Att->Attenuation.bSpatialize = true;
    Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    Att->Attenuation.AttenuationShapeExtents = FVector(250.f, 0.f, 0.f);
    Att->Attenuation.FalloffDistance = 1700.f;
    Horn = UGameplayStatics::SpawnSoundAttached(Wave, Moving, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true, .75f, 1.f, 0.f, Att, nullptr, false);
}

void AHouseGadget::StopHorn()
{
    if (!Horn) return;
    Horn->Stop();
    Horn->DestroyComponent();
    Horn = nullptr;
}

void AHouseGadget::ApplyState(bool bWithSound)
{
    if (!bBuilt) BuildParts();
    const FVector At = Moving ? Moving->GetComponentLocation() : GetActorLocation();
    switch (Kind)
    {
    case EHouseGadgetKind::HornStrobe:
        if (State) StartHorn(); else StopHorn();
        if (Glow && !State) Glow->SetVectorParameterValue(TEXT("Emission"), FLinearColor::Black);
        break;
    case EHouseGadgetKind::LightSwitch:
        FindLights();
        for (const TWeakObjectPtr<ULightComponent>& W : Lights)
            if (ULightComponent* L = W.Get()) L->SetVisibility(State == 0);
        if (bWithSound) HouseSfx::Click(GetWorld(), At, 0);
        break;
    case EHouseGadgetKind::PullStation:
        if (bWithSound && State) HouseSfx::Click(GetWorld(), At, 1);
        break;
    case EHouseGadgetKind::BillyBass:
        if (IsValid(Song)) { Song->Stop(); Song->DestroyComponent(); }
        Song = nullptr;
        if (State && bWithSound) { HouseSfx::Click(GetWorld(), At, 2); Song = HouseSfx::Song(Moving); }
        break;
    default:
        break;
    }
}

void AHouseGadget::BuildParts()
{
    if (bBuilt || !GetWorld()) return;
    UHouseBlackHallSubsystem* Look = GetWorld()->GetSubsystem<UHouseBlackHallSubsystem>();
    if (!Look) return;
    Look->CreateLook();
    HwCreateLook(Look);
    auto Reg = [Look](const TCHAR* Key, const FLinearColor& Tint, float Rough) { if (!Look->Materials.Contains(Key)) HallMakePlain(Look, Key, Tint, Rough); };
    Reg(TEXT("HW_BassBack"), FLinearColor(.10f, .17f, .08f), .45f);
    Reg(TEXT("HW_BassBelly"), FLinearColor(.58f, .62f, .22f), .5f);
    Reg(TEXT("HW_Santa"), FLinearColor(.62f, .03f, .03f), .85f);
    Reg(TEXT("HW_Fur"), FLinearColor(.92f, .9f, .86f), .95f);
    Reg(TEXT("HW_FishEye"), FLinearColor(.02f, .02f, .02f), .15f);
    bBuilt = true;

    const FHallUV UV = FHallUV::World(50.f);
    FHallMesh FixedM, MovingM, Moving2M, Moving3M;
    FVector FocusMin(-1, -7, 0), FocusMax(9, 7, 14);
    switch (Kind)
    {
    case EHouseGadgetKind::PullStation:
        // Handle on a hinge along its top edge; the body stays in the hallway decor.
        Hinge->SetRelativeLocation(FVector(6.5f, 0, 6.6f));
        MovingM.BoxMinMax(TEXT("HW_White"), FVector(0, -4.2f, -2.f), FVector(1.7f, 4.2f, 0.f), UV);
        break;
    case EHouseGadgetKind::HornStrobe:
        MovingM.BoxMinMax(TEXT("HW_Lens"), FVector(7.f, -4.5f, 9.2f), FVector(9.2f, 4.5f, 13.4f), UV);
        FocusMin = FVector(-1, -7, 0); FocusMax = FVector(10, 7, 15);
        break;
    case EHouseGadgetKind::LightSwitch:
        Hinge->SetRelativeLocation(FVector(.6f, 0, 0));
        MovingM.BoxMinMax(TEXT("HW_White"), FVector(0, -.5f, -1.2f), FVector(.9f, .5f, 1.2f), UV);
        FocusMin = FVector(-1, -4.5f, -6.5f); FocusMax = FVector(3, 4.5f, 6.5f);
        break;
    case EHouseGadgetKind::BillyBass:
    {
        // Textured largemouth bass on a molded shield plaque; body swings out from the tail, jaw and tail move on their own hinges.
        FHwBassParts Parts;
        HwBuildBillyBass(Look, Parts);
        FixedM = MoveTemp(Parts.Plaque);
        MovingM = MoveTemp(Parts.Body);
        Moving2M = MoveTemp(Parts.Tail);
        Moving3M = MoveTemp(Parts.Jaw);
        Hinge->SetRelativeLocation(Parts.BodyPivot);
        Hinge2->SetRelativeLocation(Parts.TailPivot - Parts.BodyPivot);
        Hinge3->SetRelativeLocation(Parts.JawPivot - Parts.BodyPivot);
        BassHeadSign = Parts.HeadSign;
        for (UProceduralMeshComponent* PM : { Fixed.Get(), Moving.Get(), Moving2.Get(), Moving3.Get() }) PM->SetCastShadow(true);
        FocusMin = FVector(-1, -20, -14); FocusMax = FVector(15, 20, 22);
        break;
    }
    default:
        break;
    }
    auto Apply = [Look](const FHallMesh& M, UProceduralMeshComponent* PM)
    {
        if (M.NumTriangles() == 0) { PM->SetVisibility(false); return; }
        M.Build(PM, [Look](FName Key) { return Look->Mat(Key); }, false);
    };
    Apply(FixedM, Fixed);
    Apply(MovingM, Moving);
    Apply(Moving2M, Moving2);
    Apply(Moving3M, Moving3);
    Focus->SetBoxExtent((FocusMax - FocusMin) * .5f);
    Focus->SetRelativeLocation((FocusMax + FocusMin) * .5f);
    if (Kind == EHouseGadgetKind::HornStrobe && !Flash && GetNetMode() != NM_DedicatedServer)
    {
        // The strobe throws light on the hall for the frames it is lit (visibility toggled in Tick).
        Flash = NewObject<UPointLightComponent>(this, TEXT("Flash"));
        Flash->SetupAttachment(Root);
        Flash->SetMobility(EComponentMobility::Movable);
        Flash->SetRelativeLocation(FVector(34.f, 0.f, 14.f));
        Flash->SetIntensityUnits(ELightUnits::Candelas);
        Flash->SetIntensity(110.f);
        Flash->SetLightColor(FLinearColor(1.f, 1.f, 1.f));
        Flash->SetAttenuationRadius(520.f);
        Flash->SetCastShadows(false);
        Flash->MaxDrawDistance = 2600.f;
        Flash->MaxDistanceFadeRange = 400.f;
        Flash->SetVisibility(false);
        Flash->RegisterComponent();
    }
    if (Kind == EHouseGadgetKind::HornStrobe)
        if (UMaterialInterface* Lens = Look->Mat(TEXT("HW_Lens")))
        {
            // A dynamic instance cannot parent another one: build the glow from the lens's base material and copy its look.
            UMaterialInstanceDynamic* LensMID = Cast<UMaterialInstanceDynamic>(Lens);
            if (UMaterialInterface* Base = LensMID ? LensMID->Parent.Get() : Lens)
            {
                Glow = UMaterialInstanceDynamic::Create(Base, this);
                if (LensMID) Glow->CopyParameterOverrides(LensMID);
                Moving->SetMaterial(0, Glow);
            }
        }
}

void AHouseGadget::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bBuilt) { BuildParts(); if (!bBuilt) return; }
    const float T = FMath::Max(0.f, ServerNow() - StateTime);
    switch (Kind)
    {
    case EHouseGadgetKind::PullStation:
        Pitch = FMath::FInterpTo(Pitch, State ? 48.f : 0.f, DeltaSeconds, 10.f);
        Hinge->SetRelativeRotation(FRotator(Pitch, 0, 0));
        break;
    case EHouseGadgetKind::LightSwitch:
        Pitch = FMath::FInterpTo(Pitch, State ? -22.f : 22.f, DeltaSeconds, 14.f);
        Hinge->SetRelativeRotation(FRotator(Pitch, 0, 0));
        break;
    case EHouseGadgetKind::HornStrobe:
        bStrobeLit = State != 0 && FMath::Fmod(T, 1.f) < .12f;
        if (Glow && State) Glow->SetVectorParameterValue(TEXT("Emission"), bStrobeLit ? FLinearColor(60.f, 60.f, 70.f) : FLinearColor::Black);
        if (Flash && Flash->IsVisible() != bStrobeLit) Flash->SetVisibility(bStrobeLit);
        break;
    case EHouseGadgetKind::BillyBass:
    {
        // Head turns out from the plaque, mouth follows the song, tail flicks (outward only, so it never clips the board).
        float Swing = 0.f, Flap = 0.f, Mouth = 0.f;
        if (State)
        {
            const float Env = FMath::Min(FMath::Clamp(T / .45f, 0.f, 1.f), FMath::Clamp((5.2f - T) / .45f, 0.f, 1.f));
            Swing = 30.f * Env * (.9f + .1f * FMath::Sin(T * 7.f));
            Mouth = HwBassMouth(T);
            Flap = Env * (9.f + 12.f * FMath::Sin(T * 2 * PI * 3.f)) * (Mouth > .05f ? .35f : 1.f);
        }
        Hinge->SetRelativeRotation(FRotator(0, -BassHeadSign * Swing, 0));
        // The tail rides on the body; counter-rotate by the swing so it stays clear of the board, then flick outward.
        Hinge2->SetRelativeRotation(FRotator(0, BassHeadSign * Swing + FMath::Max(0.f, Flap), 0));
        Hinge3->SetRelativeRotation(FQuat(FVector::XAxisVector, -BassHeadSign * FMath::DegreesToRadians(24.f * Mouth)));
        break;
    }
    default:
        break;
    }
}
