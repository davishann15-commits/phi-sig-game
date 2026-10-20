#include "HouseWoodcutting.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseDetail.h"
#include "HouseFurnitureInteractionComponent.h"
#include "HouseWoodsDetail.h"
#include "StoryCampaign.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
constexpr float SwingDuration = .62f;
const FHallUV BarkUV = FHallUV::World(100), WoodUV = FHallUV::World(80), Plain = FHallUV::World(60);

void LookForTools(UHouseBlackHallSubsystem* L)
{
    if (L->Materials.Contains(TEXT("AX_Steel"))) return;
    HallMakePlain(L, TEXT("AX_Steel"), FLinearColor(.105f, .12f, .13f), .42f, .82f);
    HallMakePlain(L, TEXT("AX_Edge"), FLinearColor(.42f, .46f, .48f), .23f, .94f);
    HallMakePlain(L, TEXT("AX_Grain"), FLinearColor(.14f, .061f, .022f), .92f);
    HallMakePlain(L, TEXT("AX_Crack"), FLinearColor(.025f, .016f, .009f), 1);
    // True radial end grain: growth rings, saw marks and checks, not a wood plank texture.
    FHallImage Cut, Normal; constexpr int N = 256; Cut.Init(N, N, FLinearColor::Black);
    TArray<float> H; H.SetNum(N * N);
    for (int Y = 0; Y < N; ++Y) for (int X = 0; X < N; ++X)
    {
        const float DX = (X - 125.f) / 128, DY = (Y - 132.f) / 128;
        const float R = FMath::Sqrt(DX * DX + DY * DY), A = FMath::Atan2(DY, DX);
        const float Ring = .5f + .5f * FMath::Sin(R * 112 + 1.8f * FMath::Sin(A * 4) + HallFbm(X * .028f, Y * .028f, 3, 241) * 4);
        const float Fine = HallNoise(X * 1.4f, Y * 1.4f, 251);
        const float Saw = .5f + .5f * FMath::Sin(X * .57f + Y * .041f);
        const float V = .40f + .14f * Ring + .035f * Fine + .016f * Saw - .07f * FMath::SmoothStep(.8f, 1.f, R);
        Cut.At(X, Y) = FLinearColor(V, V * .71f, V * .39f); H[Y * N + X] = Ring * .07f + Fine * .055f + Saw * .01f;
    }
    HallHeightToNormal(H, N, N, 1.2f, true, Normal);
    auto* T = HallMakeTexture(Cut, true, false, TEXT("T_ColeStumpEndGrain"));
    auto* TN = HallMakeTexture(Normal, false, true, TEXT("T_ColeStumpEndGrainN"));
    L->Textures.Add(TEXT("T_ColeStumpEndGrain"), T); L->Textures.Add(TEXT("T_ColeStumpEndGrainN"), TN);
    HallMakeTextured(L, TEXT("AX_EndGrain"), T, TN, .91f, FLinearColor::White, .8f);
}

float Radius(float A, int Layer)
{
    const float Profile[] = {43, 39, 34, 32.5f, 33};
    return Profile[Layer] + 1.2f * FMath::Sin(A * 5 + .6f) + .65f * FMath::Sin(A * 13 - .3f) + .30f * FMath::Sin(A * 29);
}

void StumpMesh(FHallMesh& M)
{
    constexpr int Seg = 64; const float Z[] = {-4, 4, 17, 38, 54};
    auto P = [&](float A, int Row) { return FVector(FMath::Cos(A) * Radius(A, Row), FMath::Sin(A) * Radius(A, Row), Z[Row]); };
    for (int Row = 0; Row < 4; ++Row) for (int I = 0; I < Seg; ++I)
    {
        const float A = 2 * PI * I / Seg, B = 2 * PI * (I + 1) / Seg;
        M.Quad(TEXT("EX_NatureBark"), P(A, Row), P(B, Row), P(B, Row + 1), P(A, Row + 1), FVector(FMath::Cos((A + B) * .5f), FMath::Sin((A + B) * .5f), .12f).GetSafeNormal(), BarkUV);
    }
    // A single cap with explicit planar UVs puts the radial texture exactly on the cut face.
    auto& S = M.Sec(TEXT("AX_EndGrain")); const int Base = S.V.Num();
    S.V.Add(FVector(0, 0, 54)); S.N.Add(FVector::UpVector); S.UV.Add(FVector2D(.5f)); S.T.Add(FProcMeshTangent(FVector::ForwardVector, false));
    for (int I = 0; I < Seg; ++I)
    {
        const FVector V = P(2 * PI * I / Seg, 4);
        S.V.Add(V); S.N.Add(FVector::UpVector); S.UV.Add(FVector2D(V.X / 70 + .5f, V.Y / 70 + .5f)); S.T.Add(FProcMeshTangent(FVector::ForwardVector, false));
        S.I.Append({Base, Base + 1 + (I + 1) % Seg, Base + 1 + I});
    }
    // Root flare, chipped bark and narrow age cracks ground the block in the dirt.
    for (int I = 0; I < 7; ++I)
    {
        const float A = I * 2 * PI / 7 + .2f; const FVector D(FMath::Cos(A), FMath::Sin(A), 0);
        M.Cylinder(TEXT("EX_NatureBark"), D * 25 + FVector(0, 0, 13), D * 48 + FVector(0, 0, 3), 5.5f, 9, BarkUV);
        M.Cylinder(TEXT("EX_NatureBark"), D * 46 + FVector(0, 0, 3), D * 59 + FVector(0, 0, -1), 2.8f, 8, BarkUV);
    }
    for (int I = 0; I < 5; ++I)
    {
        const float A = I * 1.17f + .3f; const FVector D(FMath::Cos(A), FMath::Sin(A), 0);
        const float End = 23 + I % 3 * 3;
        M.Cylinder(TEXT("AX_Crack"), D * 32 + FVector(0, 0, 54.10f), D * End + FVector(0, 0, 54.10f), .18f, 4, Plain);
    }
    M.Cylinder(TEXT("AX_Crack"), FVector(-5, -9, 54.12f), FVector(12, 4, 54.12f), .36f, 4, Plain);
}

void AxeMesh(FHallMesh& M, TArray<FBox>& Collision)
{
    // Curved oval hickory handle, slightly flared at the butt. Origin is near the grip.
    constexpr int Seg = 16; const float Z[] = {-10, -6, 0, 12, 30, 48, 62, 65};
    auto P = [&](int Row, float A)
    {
        const float Width = Row < 2 ? 2.55f : Row == 7 ? 1.5f : 2.05f;
        const float Curve = 1.5f * FMath::Sin((Z[Row] + 10) / 75 * PI);
        return FVector(Curve + Width * FMath::Cos(A), 1.6f * FMath::Sin(A), Z[Row]);
    };
    for (int Row = 0; Row < 7; ++Row) for (int I = 0; I < Seg; ++I)
    {
        const float A = 2 * PI * I / Seg, B = 2 * PI * (I + 1) / Seg;
        M.Quad(TEXT("EX_WoodsCutWood"), P(Row, A), P(Row, B), P(Row + 1, B), P(Row + 1, A), FVector(FMath::Cos((A + B) * .5f), FMath::Sin((A + B) * .5f), 0), WoodUV);
    }
    M.Cylinder(TEXT("EX_WoodsCutWood"), FVector(0, 0, -10), FVector(0, 0, -9.8f), 2.45f, Seg, WoodUV);
    for (float A : {0.f, .8f, 2.f, 3.f, 4.4f})
        M.Cylinder(TEXT("AX_Grain"), P(1, A) * FVector(1.005f, 1.005f, 1), P(5, A + .035f) * FVector(1.005f, 1.005f, 1), .075f, 4, Plain);
    const TArray<FVector2D> Outline = {{-6, 52}, {-6, 63}, {4, 64}, {13, 68}, {18, 70}, {18, 48}, {6, 51}};
    M.Prism(TEXT("AX_Steel"), Outline, FVector(0, -2.5f, 0), FVector::ForwardVector, FVector::UpVector, FVector(0, 1, 0), 5, Plain);
    for (float Side : {-1.f, 1.f})
        M.Quad(TEXT("AX_Edge"), FVector(18, Side * 2.5f, 48), FVector(24, Side * .13f, 47), FVector(24, Side * .13f, 70), FVector(18, Side * 2.5f, 70), FVector(.35f, Side, 0).GetSafeNormal(), Plain);
    M.Quad(TEXT("AX_Edge"), FVector(18, -2.5f, 70), FVector(24, -.13f, 70), FVector(24, .13f, 70), FVector(18, 2.5f, 70), FVector::UpVector, Plain);
    M.Quad(TEXT("AX_Edge"), FVector(18, -2.5f, 48), FVector(18, 2.5f, 48), FVector(24, .13f, 47), FVector(24, -.13f, 47), -FVector::UpVector, Plain);
    M.Quad(TEXT("AX_Edge"), FVector(24, -.13f, 47), FVector(24, .13f, 47), FVector(24, .13f, 70), FVector(24, -.13f, 70), FVector::ForwardVector, Plain);
    M.Box(TEXT("AX_Steel"), FTransform(FVector(1, 0, 65.2f)), FVector(1.2f, .28f, .2f), Plain);
    Collision.Add(FBox(FVector(-2.6f, -1.7f, -10), FVector(3.6f, 1.7f, 52)));
    Collision.Add(FBox(FVector(-6, -2.5f, 48), FVector(24, 2.5f, 70)));
}
}

AHouseChoppingStump::AHouseChoppingStump()
{
    bReplicates = true; bAlwaysRelevant = true; SetReplicateMovement(false); SetNetUpdateFrequency(2);
    Block = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ChoppingBlock")); SetRootComponent(Block);
    Block->SetMobility(EComponentMobility::Static); Block->bUseComplexAsSimpleCollision = true;
    Block->SetCollisionProfileName(TEXT("BlockAll")); Block->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Marks = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ChopMarks")); Marks->SetupAttachment(Block);
    Marks->SetCollisionEnabled(ECollisionEnabled::NoCollision); Marks->SetCastShadow(false);
    Tags.Add(TEXT("SSOColesWoodcutting"));
}
FVector AHouseChoppingStump::Home()
{
    const float X = HouseWoods::HutX + 365, Y = HouseWoods::HutY - 340;
    return FVector(X, Y, HouseWoods::Height(X, Y));
}
void AHouseChoppingStump::BeginPlay()
{
    Super::BeginPlay();
    if (auto* L = GetWorld()->GetSubsystem<UHouseBlackHallSubsystem>())
    {
        LookForTools(L); FHallMesh M; StumpMesh(M); M.Build(Block, [L](FName K) { return L->Mat(K); }, true);
        UpdateMarks();
    }
}
void AHouseChoppingStump::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AHouseChoppingStump, Chops); }
void AHouseChoppingStump::UpdateMarks()
{
    if (!Marks || !GetWorld()) return;
    auto* L = GetWorld()->GetSubsystem<UHouseBlackHallSubsystem>(); if (!L) return;
    FHallMesh M; FRandomStream R(105126);
    for (int I = 0; I < FMath::Min(Chops, 12); ++I)
    {
        const FVector C(R.FRandRange(-16, 16), R.FRandRange(-14, 14), 54.2f);
        const FVector D = FRotator(0, R.FRandRange(0, 180), 0).Vector() * R.FRandRange(4, 9);
        M.Cylinder(TEXT("AX_Crack"), C - D, C + D, .30f, 4, Plain);
    }
    Marks->ClearAllMeshSections(); M.Build(Marks, [L](FName K) { return L->Mat(K); }, false);
}
void AHouseChoppingStump::OnRep_Chops() { UpdateMarks(); }
float AHouseChoppingStump::TakeDamage(float Amount, const FDamageEvent& Event, AController* Instigator, AActor* Causer)
{
    if (!HasAuthority() || Amount <= 0 || !Cast<AHouseWoodAxe>(Causer)) return 0;
    ++Chops; UpdateMarks(); ForceNetUpdate();
    FVector P = Top(), D = FVector::DownVector;
    if (Event.IsOfType(FPointDamageEvent::ClassID)) { const auto& E = static_cast<const FPointDamageEvent&>(Event); P = E.HitInfo.ImpactPoint; D = E.ShotDirection; }
    MulticastChop(P, D); return Amount;
}
void AHouseChoppingStump::MulticastChop_Implementation(FVector_NetQuantize Point, FVector_NetQuantizeNormal Direction)
{
    if (GetNetMode() == NM_DedicatedServer) return;
    HouseSfx::Impact(GetWorld(), Point, EHouseSurface::Wood, .85f, 30);
    auto* L = GetWorld()->GetSubsystem<UHouseBlackHallSubsystem>(); if (!L) return;
    FRandomStream R(105126 + Chops * 131); auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    for (int I = 0; I < 7; ++I)
    {
        auto* Chip = GetWorld()->SpawnActor<AActor>(); if (!Chip) continue;
        auto* C = NewObject<UStaticMeshComponent>(Chip); Chip->SetRootComponent(C); Chip->AddInstanceComponent(C);
        C->SetStaticMesh(Cube); C->SetMaterial(0, L->Mat(TEXT("EX_WoodsCutWood")));
        C->SetWorldScale3D(FVector(R.FRandRange(.018f, .038f), .009f, .012f));
        C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); C->SetCollisionResponseToAllChannels(ECR_Ignore);
        C->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); C->SetCastShadow(false); C->SetUseCCD(true); C->RegisterComponent();
        Chip->SetActorLocation(Point + FVector(R.FRandRange(-4, 4), R.FRandRange(-4, 4), 4));
        Chip->SetLifeSpan(1.2f + I * .07f);
        C->SetWorldRotation(FRotator(R.FRandRange(0, 180), R.FRandRange(0, 360), R.FRandRange(0, 180)));
        C->SetSimulatePhysics(true); C->SetMassOverrideInKg(NAME_None, .008f);
        C->SetPhysicsLinearVelocity(FVector(R.FRandRange(-80, 80), R.FRandRange(-80, 80), R.FRandRange(80, 150)));
        C->SetPhysicsAngularVelocityInRadians(FVector(R.FRandRange(-5, 5), R.FRandRange(-5, 5), R.FRandRange(-5, 5)));
    }
}

AHouseWoodAxe::AHouseWoodAxe()
{
    PropId = TEXT("ColesWoodAxe"); PropName = TEXT("chopping axe"); CarrySize = ESSOCarrySize::Small;
    MassKg = 2.2f; bMovable = true; bBreakable = false; bPhysicsActive = false;
    bAlwaysRelevant = true; SetNetUpdateFrequency(45); SetMinNetUpdateFrequency(15);
    Tags.Add(TEXT("SSOColesWoodcutting"));
}
FTransform AHouseWoodAxe::EmbeddedTransform()
{
    // Seat only the sharpened lower corner. With Unreal's pitch convention,
    // this angle raises the handle and forged head above the cut surface.
    const FRotator R(-145, 28, 0);
    const FVector Tip = AHouseChoppingStump::Home() + FVector(3, 0, 54 - StumpInsertion);
    return FTransform(R, Tip - R.RotateVector(StumpTipLocal()));
}
void AHouseWoodAxe::BeginPlay()
{
    if (auto* L = GetWorld()->GetSubsystem<UHouseBlackHallSubsystem>())
    {
        LookForTools(L); FHallMesh M; TArray<FBox> C; AxeMesh(M, C);
        IntactMesh = HwBuildStaticMesh(M, TEXT("SM_ColesChoppingAxe"), L, C); Body->SetStaticMesh(IntactMesh);
    }
    Super::BeginPlay();
    // Only deliberate swings inflict damage; carrying or dropping the axe must
    // not run the generic furniture contact-damage path as a second strike.
    Body->OnComponentHit.RemoveAll(this);
    Body->SetUseCCD(true); Body->SetLinearDamping(.32f); Body->SetAngularDamping(1.1f);
    SfxSurface = EHouseSurface::Metal; SetActorTickEnabled(true);
}
float AHouseWoodAxe::Clock() const
{ const auto* S = GetWorld()->GetGameState(); return S ? S->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds(); }
bool AHouseWoodAxe::IsSwinging() const { return Clock() - SwingStarted >= 0 && Clock() - SwingStarted < SwingDuration; }
void AHouseWoodAxe::PreparePickup()
{
    if (!HasAuthority() || !bEmbedded) return;
    bEmbedded = false; Body->SetWorldLocation(Body->GetComponentLocation() + FVector(0, 0, 14), false, nullptr, ETeleportType::TeleportPhysics);
    ForceNetUpdate();
}
bool AHouseWoodAxe::BeginSwing(AStoryFirstPersonCharacter* Holder)
{
    if (!HasAuthority() || !Holder || !Holder->FurnitureInteraction || Holder->FurnitureInteraction->GetHeldBody() != Body
        || Holder->FurnitureInteraction->GetCarryPhase() != ESSOCarryPhase::Holding || IsSwinging()) return false;
    SwingHolder = Holder; SwingStarted = Clock(); bStrikeResolved = false; ++SwingCount; SetActorTickEnabled(true); ForceNetUpdate(); return true;
}
void AHouseWoodAxe::CancelSwing() { if (HasAuthority()) { SwingHolder.Reset(); bStrikeResolved = true; SwingStarted = -10; ForceNetUpdate(); } }
void AHouseWoodAxe::CarryPose(const FVector& View, const FRotator& Aim, FVector& Grip, FQuat& Rotation) const
{
    const FVector Forward = Aim.Vector(), Right = FRotationMatrix(Aim).GetUnitAxis(EAxis::Y), Up = FRotationMatrix(Aim).GetUnitAxis(EAxis::Z);
    const float Age = Clock() - SwingStarted;
    float Angle = 24, Pull = 0;
    if (Age >= 0 && Age < SwingDuration)
    {
        if (Age < .15f) { const float T = FMath::SmoothStep(0.f, .15f, Age); Angle = FMath::Lerp(24.f, -48.f, T); Pull = T; }
        else if (Age < .34f) { const float T = FMath::SmoothStep(.15f, .34f, Age); Angle = FMath::Lerp(-48.f, 104.f, T); Pull = 1 - T; }
        else Angle = FMath::Lerp(104.f, 24.f, FMath::SmoothStep(.34f, SwingDuration, Age));
    }
    const float A = FMath::DegreesToRadians(Angle);
    Rotation = FRotationMatrix::MakeFromZY(Up * FMath::Cos(A) + Forward * FMath::Sin(A), Right).ToQuat();
    Grip = View + Forward * (48 - Pull * 7) + Right * 22 - FVector::UpVector * (22 - Pull * 8);
}
bool AHouseWoodAxe::CanReturnToStump(const AStoryFirstPersonCharacter* Holder) const
{
    if (!Holder || !Holder->FirstPersonCamera || FVector::Dist2D(Holder->GetActorLocation(), AHouseChoppingStump::Home()) > 185) return false;
    FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(AxeReturn), false, Holder); Q.AddIgnoredActor(this);
    const FVector V = Holder->FirstPersonCamera->GetComponentLocation();
    return GetWorld()->LineTraceSingleByChannel(H, V, V + Holder->GetBaseAimRotation().Vector() * 230, ECC_Visibility, Q) && Cast<AHouseChoppingStump>(H.GetActor());
}
void AHouseWoodAxe::EmbedInStump()
{
    if (!HasAuthority()) return;
    CancelSwing(); Body->SetSimulatePhysics(false); bPhysicsActive = false; bEmbedded = true;
    Body->SetWorldTransform(EmbeddedTransform(), false, nullptr, ETeleportType::TeleportPhysics);
    Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block); ForceNetUpdate();
    HouseSfx::Impact(GetWorld(), AHouseChoppingStump::Home() + FVector(0, 0, 54), EHouseSurface::Wood, .45f, 20);
}
void AHouseWoodAxe::ResolveStrike()
{
    auto* Holder = SwingHolder.Get();
    if (!Holder || !Holder->FurnitureInteraction || Holder->FurnitureInteraction->GetHeldBody() != Body || !Holder->FirstPersonCamera) return;
    const FVector V = Holder->FirstPersonCamera->GetComponentLocation(), D = Holder->GetBaseAimRotation().Vector();
    FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(AxeSwing), false, Holder); Q.AddIgnoredActor(this);
    // A short, obstructed sweep: closest surface only, never damage through walls.
    if (!GetWorld()->SweepSingleByChannel(H, V, V + D * 170, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(7), Q)) return;
    ++HitCount; if (auto* A = H.GetActor()) UGameplayStatics::ApplyPointDamage(A, 24, D, H, Holder->GetController(), this, UDamageType::StaticClass());
    if (!Cast<AHouseChoppingStump>(H.GetActor())) MulticastStrikeFx(H.ImpactPoint,
        uint8(Cast<AHouseInteractiveProp>(H.GetActor()) ? Cast<AHouseInteractiveProp>(H.GetActor())->SfxSurface : EHouseSurface::Wood));
}
void AHouseWoodAxe::MulticastStrikeFx_Implementation(FVector_NetQuantize Point, uint8 Surface)
{
    if (GetNetMode() != NM_DedicatedServer && Surface < uint8(EHouseSurface::Count))
        HouseSfx::Impact(GetWorld(), Point, EHouseSurface(Surface), .7f, 12);
}
void AHouseWoodAxe::Tick(float Delta)
{
    Super::Tick(Delta);
    if (!HasAuthority()) return;
    if (!bStrikeResolved && Clock() - SwingStarted >= .25f) { bStrikeResolved = true; ResolveStrike(); }
    if (bPhysicsActive && !Body->ComponentHasTag(TEXT("SSOFurnitureHeld"))
        && (Body->GetComponentLocation().Z < HouseWoods::Height(Body->GetComponentLocation().X, Body->GetComponentLocation().Y) - 300 || Body->GetComponentLocation().Size2D() > 26000)) EmbedInStump();
}
void AHouseWoodAxe::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AHouseWoodAxe, bEmbedded); DOREPLIFETIME(AHouseWoodAxe, SwingStarted); }

void SpawnHouseWoodcutting(UWorld* W)
{
    if (!W || W->GetNetMode() == NM_Client) return;
    TActorIterator<AHouseChoppingStump> Existing(W); if (Existing) return;
    FActorSpawnParameters P; P.ObjectFlags |= RF_Transient; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    W->SpawnActor<AHouseChoppingStump>(AHouseChoppingStump::StaticClass(), FTransform(AHouseChoppingStump::Home()), P);
    W->SpawnActor<AHouseWoodAxe>(AHouseWoodAxe::StaticClass(), AHouseWoodAxe::EmbeddedTransform(), P);
}
