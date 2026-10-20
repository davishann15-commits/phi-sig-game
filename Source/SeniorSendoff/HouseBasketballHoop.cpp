#include "HouseBasketballHoop.h"
#include "HouseBasketball.h"
#include "HouseBasketballCourt.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallMesh.h"
#include "HouseSfx.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

namespace
{
constexpr int32 Columns = 12, Rows = 6;
constexpr float StepTime = 1.f / 120.f;
int Id(int I, int J) { return J * Columns + (I + Columns) % Columns; }
}

AHouseBasketballHoop::AHouseBasketballHoop()
{
    bReplicates = true; bAlwaysRelevant = true; SetReplicateMovement(false);
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("HoopOrigin"));
    RootComponent->SetMobility(EComponentMobility::Static);
    Net = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MovingRopeNet"));
    Net->SetupAttachment(RootComponent); Net->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Net->SetCanEverAffectNavigation(false); Net->SetCastShadow(false);
    RimMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RimVisual"));
    RimMesh->SetupAttachment(RootComponent); RimMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RimMesh->SetCanEverAffectNavigation(false); RimMesh->SetCastShadow(false);
}

void AHouseBasketballHoop::BeginPlay()
{
    Super::BeginPlay();
    Metal = NewObject<UPhysicalMaterial>(this);
    Metal->Friction = .28f; Metal->Restitution = .64f;
    Metal->RestitutionCombineMode = EFrictionCombineMode::Average;
    Metal->bOverrideRestitutionCombineMode = true;
    // Separate, rounded capsules: not a filled disc or a faceted collision hull.
    FHallMesh RimArt;
    for (int I = 0; I < 48; ++I)
    {
        const float A = 2 * PI * I / 48, B = 2 * PI * (I + 1) / 48;
        const FVector From(24.2f * FMath::Cos(A), 24.2f * FMath::Sin(A), 0);
        const FVector To(24.2f * FMath::Cos(B), 24.2f * FMath::Sin(B), 0);
        RimArt.Cylinder(TEXT("EX_CourtRim"), From, To, 1.25f, 8, FHallUV::World(20));
        auto* C = NewObject<UCapsuleComponent>(this, *FString::Printf(TEXT("RimContact%d"), I));
        AddInstanceComponent(C); C->SetupAttachment(RootComponent);
        C->SetMobility(EComponentMobility::Static);
        C->SetCapsuleSize(1.25f, FVector::Dist(From, To) * .5f + 1.25f);
        C->SetRelativeLocation((From + To) * .5f);
        C->SetRelativeRotation(FRotationMatrix::MakeFromZ(To - From).Rotator());
        C->SetCollisionProfileName(TEXT("BlockAll"));
        C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        C->SetPhysMaterialOverride(Metal); C->SetCanEverAffectNavigation(false);
        C->ComponentTags.Add(TEXT("CourtRim")); C->RegisterComponent();
    }
    auto* Board = NewObject<UBoxComponent>(this, TEXT("BackboardContact"));
    AddInstanceComponent(Board); Board->SetupAttachment(RootComponent);
    Board->SetMobility(EComponentMobility::Static);
    Board->SetRelativeLocation(FVector(End() == 0 ? -31 : 31, 0, 38));
    Board->SetBoxExtent(FVector(2, 91, 54)); Board->SetCollisionProfileName(TEXT("BlockAll"));
    Board->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Board->SetPhysMaterialOverride(Metal); Board->SetCanEverAffectNavigation(false);
    Board->ComponentTags.Add(TEXT("CourtBackboard")); Board->RegisterComponent();
    if (auto* Look = GetWorld()->GetSubsystem<UHouseBlackHallSubsystem>())
        RimArt.Build(RimMesh, [Look](FName K) { return Look->Mat(K); }, false);

    for (int J = 0; J < Rows; ++J) for (int I = 0; I < Columns; ++I)
    {
        const float A = 2 * PI * (I + (J % 2 ? .5f : 0.f)) / Columns;
        const float Radius = 24.f - FMath::Min(J, 3) * 3.f - FMath::Max(J - 3, 0) * 1.6f;
        Rest.Add(FVector(Radius * FMath::Cos(A), Radius * FMath::Sin(A), -J * 9.f - .4f));
    }
    P = Prev = Rest;
    auto Link = [&](int A, int B) { Links.Add({A, B, float(FVector::Dist(Rest[A], Rest[B]))}); };
    for (int J = 0; J < Rows - 1; ++J) for (int I = 0; I < Columns; ++I)
    {
        Link(Id(I, J), Id(I, J + 1));
        Link(Id(I, J), Id(I + (J % 2 ? 1 : -1), J + 1));
    }
    UpdateNetMesh(true);
}

void AHouseBasketballHoop::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    CacheAge += DeltaSeconds; Clock += FMath::Min(DeltaSeconds, .05f);
    if (CacheAge > 1)
    {
        CacheAge = 0; Balls.Reset();
        for (TActorIterator<AHouseBasketball> It(GetWorld()); It; ++It) Balls.Add(*It);
    }
    TArray<FBallContact> Contacts;
    for (const auto& Weak : Balls) if (auto* Ball = Weak.Get(); Ball && Ball->Body)
    {
        const FVector At = Ball->Body->GetComponentLocation() - GetActorLocation();
        const FVector Before = LastBallPositions.Contains(Weak) ? LastBallPositions[Weak] : At;
        LastBallPositions.Add(Weak, At);
        if (FMath::Min(At.SizeSquared(), Before.SizeSquared()) < 125 * 125)
            Contacts.Add({At, Before, float(Ball->Body->Bounds.BoxExtent.GetMax()) + .5f});
    }
    // Cap catch-up after shader stalls. The anchored cloth never explodes on a long frame.
    Accumulator = FMath::Min(Accumulator + DeltaSeconds, StepTime * 6);
    const int Steps = FMath::FloorToInt(Accumulator / StepTime);
    for (int I = 0; I < Steps; ++I)
    { Step(StepTime, Contacts, (I + 1.f) / Steps); Accumulator -= StepTime; }
    MeshAge += DeltaSeconds;
    if (GetNetMode() != NM_DedicatedServer && MeshAge >= 1.f / 45.f)
    { MeshAge = 0; UpdateNetMesh(false); }
}

void AHouseBasketballHoop::Step(float Dt, const TArray<FBallContact>& Contacts, float Fraction)
{
    if (P.Num() != Columns * Rows) return;
    for (int I = Columns; I < P.Num(); ++I)
    {
        const FVector Velocity = (P[I] - Prev[I]) * .986f;
        Prev[I] = P[I];
        const float Height = (I / Columns) / float(Rows - 1);
        const FVector Air(FMath::Sin(Clock * 1.7f + I * .07f) * 10, FMath::Sin(Clock * 1.1f) * 6, -35);
        P[I] += Velocity + ((Rest[I] - P[I]) * 20 + Air * Height) * Dt * Dt;
    }
    for (int Pass = 0; Pass < 7; ++Pass)
    {
        for (const auto& L : Links)
        {
            const FVector D = P[L.B] - P[L.A]; const float Length = D.Size();
            if (Length < .001f) continue;
            const float A = L.A >= Columns ? 1.f : 0.f, B = L.B >= Columns ? 1.f : 0.f;
            const FVector Correction = D * ((Length - L.Length) / Length) * .92f / (A + B);
            P[L.A] += Correction * A; P[L.B] -= Correction * B;
        }
        for (const auto& Ball : Contacts)
        {
            const FVector Center = FMath::Lerp(Ball.Before, Ball.Center, Fraction);
            // Particle and midpoint contacts keep the actual rope outside the ball,
            // rather than playing a canned net animation on a score event.
            for (int I = Columns; I < P.Num(); ++I)
            {
                FVector D = P[I] - Center; const float Length = D.Size();
                if (Length >= Ball.Radius || Length < .001f) continue;
                const FVector Push = D * ((Ball.Radius - Length) / Length);
                P[I] += Push; if (Pass == 0) { Prev[I] += Push * .7f; ++NetContacts; }
            }
            for (const auto& L : Links)
            {
                const FVector Mid = (P[L.A] + P[L.B]) * .5f, D = Mid - Center;
                const float Length = D.Size();
                if (Length >= Ball.Radius || Length < .001f) continue;
                const FVector Push = D * ((Ball.Radius - Length) / Length);
                const float A = L.A >= Columns ? 1.f : 0.f, B = L.B >= Columns ? 1.f : 0.f;
                if (A + B > 0) { P[L.A] += Push * A * 2 / (A + B); P[L.B] += Push * B * 2 / (A + B); }
            }
        }
        for (int I = 0; I < Columns; ++I) P[I] = Prev[I] = Rest[I];
    }
    for (int I = Columns; I < P.Num(); ++I)
    {
        if (P[I].ContainsNaN()) P[I] = Prev[I] = Rest[I];
        P[I] = Rest[I] + (P[I] - Rest[I]).GetClampedToMaxSize(55);
    }
}

void AHouseBasketballHoop::UpdateNetMesh(bool First)
{
    FHallMesh Mesh;
    for (const auto& L : Links)
        Mesh.Cylinder(TEXT("EX_CourtNet"), P[L.A], P[L.B], .40f, 5, FHallUV::World(12));
    if (First)
    {
        if (auto* Look = GetWorld()->GetSubsystem<UHouseBlackHallSubsystem>())
            Mesh.Build(Net, [Look](FName K) { return Look->Mat(K); }, false);
    }
    else if (const auto* S = Mesh.Sections.Find(TEXT("EX_CourtNet")))
        Net->UpdateMeshSection(0, S->V, S->N, S->UV, TArray<FColor>(), S->T);
}

void AHouseBasketballHoop::PushNet(const FVector& Point, const FVector& Velocity)
{
    const FVector At = Point - GetActorLocation(), V = Velocity.GetClampedToMaxSize(250);
    for (int I = Columns; I < P.Num(); ++I)
        Prev[I] -= V * StepTime * FMath::Exp(-FVector::DistSquared(P[I], At) / 1400.f);
}
float AHouseBasketballHoop::NetDisplacement() const
{ float Max = 0; for (int I = 0; I < P.Num(); ++I) Max = FMath::Max(Max, float(FVector::Dist(P[I], Rest[I]))); return Max; }
float AHouseBasketballHoop::PinError() const
{ float Max = 0; for (int I = 0; I < FMath::Min(Columns, P.Num()); ++I) Max = FMath::Max(Max, float(FVector::Dist(P[I], Rest[I]))); return Max; }

void AHouseBasketballHoop::BallImpact(UPrimitiveComponent* Surface, const FVector& Point, const FVector& Velocity, float Speed)
{
    if (!HasAuthority() || !Surface || Speed < 75) return;
    const bool Board = Surface->ComponentHasTag(TEXT("CourtBackboard"));
    double& Last = Board ? LastBoardSound : LastRimSound;
    if (GetWorld()->GetTimeSeconds() - Last < .095) return;
    Last = GetWorld()->GetTimeSeconds();
    MulticastContact(Point, Velocity, Board ? 1 : 0, uint8(FMath::Clamp(Speed / 5, 1.f, 255.f)));
}
void AHouseBasketballHoop::Basket(const FVector& Velocity)
{
    if (HasAuthority()) MulticastContact(GetActorLocation() - FVector(0, 0, 20), Velocity, 2, 180);
}
void AHouseBasketballHoop::MulticastContact_Implementation(FVector_NetQuantize Point, FVector_NetQuantize Velocity, uint8 Kind, uint8 Strength)
{
    if (Kind == 0) ++RimHits; else if (Kind == 1) ++BoardHits; else ++Swishes;
    PushNet(Point, Kind == 2 ? FVector(Velocity).GetClampedToMaxSize(180) : FVector(Velocity) * .18f);
    if (HouseSfx::Basketball(GetWorld(), Point, Kind + 1, Strength * 5.f)) ++AudioEvents;
}

void SpawnHouseBasketballHoops(UWorld* World)
{
    if (!World || World->GetNetMode() == NM_Client) return;
    TActorIterator<AHouseBasketballHoop> Existing(World);
    if (Existing) return;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags |= RF_Transient;
    for (int I = 0; I < 2; ++I)
        World->SpawnActor<AHouseBasketballHoop>(HouseCourt::Rim(I), FRotator::ZeroRotator, Params);
}
