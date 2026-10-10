#include "HouseExitSign.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace { int32 SmokeFinished = 0, SmokeFailed = 0; }
AHouseExitSign::AHouseExitSign()
{
    PropName = TEXT("illuminated exit sign");
    bWallMounted = true; bMovable = true; bBreakable = true;
    DetachHealth = 8.f; BreakHealth = 24.f; MassKg = 2.8f;
    CarrySize = ESSOCarrySize::Small; DamageCategory = TEXT("Fixtures");
    Tags.Add(TEXT("HouseExitSign"));
    Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Red lens spill"));
    Glow->SetupAttachment(Body);
    Glow->SetRelativeLocation(FVector(0, 6.f, 0));
    Glow->SetLightColor(FLinearColor(1.f, .005f, .002f));
    Glow->SetIntensity(18.f); Glow->SetAttenuationRadius(95.f);
    Glow->SetCastShadows(false);
}
void AHouseExitSign::Configure(uint8 Direction, bool DoubleSided)
{
    ArrowStyle = Direction; bDoubleSided = DoubleSided; ApplyStyle();
}
void AHouseExitSign::ApplyStyle()
{
    const TCHAR* Variant = ArrowStyle == 1 ? TEXT("L") : ArrowStyle == 2 ? TEXT("R") : TEXT("0");
    const FString Stem = FString::Printf(TEXT("/Game/SeniorSendOff/House/ExitSigns/SM_NEX_%s_%s_"), bDoubleSided ? TEXT("Double") : TEXT("Wall"), Variant);
    IntactMesh = LoadObject<UStaticMesh>(nullptr, *(Stem + TEXT("0")));
    DamagedMesh = LoadObject<UStaticMesh>(nullptr, *(Stem + TEXT("1")));
    HeavilyDamagedMesh = LoadObject<UStaticMesh>(nullptr, *(Stem + TEXT("2")));
    BrokenMesh = LoadObject<UStaticMesh>(nullptr, *(Stem + TEXT("3")));
    MaterialOverrides.Empty(); RefreshProp(); UpdateGlow();
}
void AHouseExitSign::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform); ApplyStyle();
}
void AHouseExitSign::OnRep_Style() { ApplyStyle(); }
void AHouseExitSign::BeginPlay()
{
    ApplyStyle(); Super::BeginPlay(); UpdateGlow();
    // Small local spill preserves the glowing red lens on low graphics settings.
    GetWorldTimerManager().SetTimer(GlowTimer, this, &AHouseExitSign::UpdateGlow, .2f, true);
    UE_LOG(LogTemp, Display, TEXT("HOUSE_NEON_EXIT_READY id=%s double=%d direction=%d"), *PropId.ToString(), bDoubleSided, ArrowStyle);
#if !UE_BUILD_SHIPPING
    if (HasAuthority() && FParse::Param(FCommandLine::Get(), TEXT("HouseExitSignSmoke")))
    {
        SmokeStart = Body->GetComponentLocation();
        GetWorldTimerManager().SetTimer(SmokeTimer, this, &AHouseExitSign::SmokeImpact, 8.f, false);
    }
#endif
}
void AHouseExitSign::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(GlowTimer); GetWorldTimerManager().ClearTimer(SmokeTimer);
    Super::EndPlay(Reason);
}
void AHouseExitSign::UpdateGlow()
{
    if (Glow) Glow->SetIntensity(bBroken || DamageStage >= 2 ? 0.f : DamageStage == 1 ? 10.f : 18.f);
}
void AHouseExitSign::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AHouseExitSign, ArrowStyle); DOREPLIFETIME(AHouseExitSign, bDoubleSided);
}
void AHouseExitSign::SmokeImpact()
{
    FHitResult Hit;
    FVector Front = GetActorRightVector();
    const FVector Center = Body->Bounds.Origin;
    bool bTarget = GetWorld()->LineTraceSingleByChannel(Hit, Center + Front * 100.f, Center - Front * 10.f, ECC_Visibility) && Hit.GetActor() == this;
    // End-of-hall pendants can sit in front of a wall: test the reachable face.
    if (!bTarget && bDoubleSided)
    {
        Front = -Front;
        bTarget = GetWorld()->LineTraceSingleByChannel(Hit, Center + Front * 100.f, Center - Front * 10.f, ECC_Visibility) && Hit.GetActor() == this;
    }
    if (!bTarget) { ++SmokeFailed; Hit.ImpactPoint = Center; }
    UE_LOG(LogTemp, Display, TEXT("HOUSE_NEON_EXIT_TARGET_%s id=%s"), bTarget ? TEXT("PASS") : TEXT("FAIL"), *PropId.ToString());
    const FVector Direction = -Front;
    UGameplayStatics::ApplyPointDamage(this, 12.f, Direction, Hit, nullptr, nullptr, nullptr);
    GetWorldTimerManager().SetTimer(SmokeTimer, this, &AHouseExitSign::SmokeCheckFall, 4.f, false);
}
void AHouseExitSign::SmokeCheckFall()
{
    const bool bPass = bDetachedFromWall && Body->IsSimulatingPhysics() && Body->GetComponentLocation().Z < SmokeStart.Z - 50.f;
    if (!bPass) ++SmokeFailed;
    UE_LOG(LogTemp, Display, TEXT("HOUSE_NEON_EXIT_FALL_%s id=%s drop=%.1f sim=%d"), bPass ? TEXT("PASS") : TEXT("FAIL"), *PropId.ToString(), SmokeStart.Z - Body->GetComponentLocation().Z, Body->IsSimulatingPhysics());
    UGameplayStatics::ApplyDamage(this, 100.f, nullptr, nullptr, nullptr);
    GetWorldTimerManager().SetTimer(SmokeTimer, this, &AHouseExitSign::SmokeCheckBreak, 1.f, false);
}
void AHouseExitSign::SmokeCheckBreak()
{
    const bool bPass = bBroken && Body->GetStaticMesh() == BrokenMesh && Glow->Intensity == 0.f && Body->IsSimulatingPhysics();
    if (!bPass) ++SmokeFailed;
    UE_LOG(LogTemp, Display, TEXT("HOUSE_NEON_EXIT_BREAK_%s id=%s light=%.1f"), bPass ? TEXT("PASS") : TEXT("FAIL"), *PropId.ToString(), Glow->Intensity);
    if (++SmokeFinished == 15)
    {
        UE_LOG(LogTemp, Display, TEXT("HOUSE_NEON_EXIT_SMOKE_%s signs=%d failures=%d"), SmokeFailed == 0 ? TEXT("PASS") : TEXT("FAIL"), SmokeFinished, SmokeFailed);
        FPlatformMisc::RequestExitWithStatus(false, SmokeFailed == 0 ? 0 : 1);
    }
}
