#include "HouseBlackHall.h"
#include "HouseWorldScale.h"
#include "HouseBlackHallLayout.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"

#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ProceduralMeshComponent.h"

using namespace HallSpec;

namespace
{
struct FHallLitterPhys { float Mass, Friction, Restitution, LinDamp, AngDamp, KickScale; };
const FHallLitterPhys HallLitterPhysTable[AHouseBlackHallLitter::KindCount] = {
    { .05f, .5f, .3f, 1.0f, 1.1f, 1.0f },     // Can
    { .045f, .65f, .15f, 1.3f, 1.4f, .8f },   // CrushedCan
    { .03f, .55f, .12f, .95f, 1.2f, .85f },   // Cup
    { .02f, .9f, .05f, 1.2f, 1.8f, .65f },    // Paper
    { .06f, .5f, .25f, 1.0f, 1.1f, .9f },     // Bottle
    { .02f, .85f, .05f, 1.6f, 2.f, .45f },    // Plate
    { .04f, .8f, .05f, 1.4f, 1.6f, .5f },     // ChipBag
    { .35f, .7f, .05f, 1.f, 1.5f, .3f },      // PizzaBox
    { 75.f, .8f, .05f, .5f, 2.f, 0.f },       // Keg (in an ice tub)
};

/** Open frustum between A (radius RA) and B (radius RB); bInward makes it visible from inside. V runs 0..1 from A to B. */
void HallFrustum(FHallMesh& M, FName Key, const FVector& A, const FVector& B, float RA, float RB, int32 Seg, bool bInward)
{
    const FVector Ax = (B - A).GetSafeNormal();
    const FVector X = FVector::CrossProduct(Ax, FMath::Abs(Ax.Z) < .9f ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
    const FVector Y = FVector::CrossProduct(Ax, X);
    const float Slope = (RA - RB) / FMath::Max(1e-3f, (B - A).Size());
    for (int32 K = 0; K < Seg; ++K)
    {
        const float A0 = 2 * PI * K / Seg, A1 = 2 * PI * (K + 1) / Seg;
        const FVector D0 = X * FMath::Cos(A0) + Y * FMath::Sin(A0), D1 = X * FMath::Cos(A1) + Y * FMath::Sin(A1);
        const FVector N0 = ((D0 + Ax * Slope).GetSafeNormal()) * (bInward ? -1.f : 1.f), N1 = ((D1 + Ax * Slope).GetSafeNormal()) * (bInward ? -1.f : 1.f);
        FHallSection& S = M.Sec(Key);
        const int32 Base = S.V.Num();
        const FVector P[4] = { A + D0 * RA, A + D1 * RA, B + D1 * RB, B + D0 * RB };
        const FVector N[4] = { N0, N1, N1, N0 };
        const FVector2D UV[4] = { FVector2D(K / float(Seg), 0), FVector2D((K + 1) / float(Seg), 0), FVector2D((K + 1) / float(Seg), 1), FVector2D(K / float(Seg), 1) };
        for (int32 Q = 0; Q < 4; ++Q) { S.V.Add(P[Q]); S.N.Add(N[Q]); S.UV.Add(UV[Q]); S.T.Add(FProcMeshTangent((D1 - D0).GetSafeNormal(), false)); }
        if (FVector::DotProduct(FVector::CrossProduct(P[1] - P[0], P[2] - P[0]), N0 + N1) < 0) S.I.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
        else S.I.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
    }
}

/** Flat disc facing Nrm. */
void HallDisc(FHallMesh& M, FName Key, const FVector& C, const FVector& Nrm, float R, int32 Seg)
{
    const FVector X = FVector::CrossProduct(Nrm, FMath::Abs(Nrm.Z) < .9f ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
    const FVector Y = FVector::CrossProduct(Nrm, X);
    FHallSection& S = M.Sec(Key);
    const int32 Base = S.V.Num();
    S.V.Add(C); S.N.Add(Nrm); S.UV.Add(FVector2D(.5f, .5f)); S.T.Add(FProcMeshTangent(X, false));
    for (int32 K = 0; K <= Seg; ++K)
    {
        const float A = 2 * PI * K / Seg;
        S.V.Add(C + (X * FMath::Cos(A) + Y * FMath::Sin(A)) * R); S.N.Add(Nrm); S.UV.Add(FVector2D(.5f + .5f * FMath::Cos(A), .5f + .5f * FMath::Sin(A))); S.T.Add(FProcMeshTangent(X, false));
    }
    for (int32 K = 0; K < Seg; ++K)
    {
        const FVector P0 = S.V[Base], P1 = S.V[Base + 1 + K], P2 = S.V[Base + 2 + K];
        if (FVector::DotProduct(FVector::CrossProduct(P1 - P0, P2 - P0), Nrm) < 0) S.I.Append({ Base, Base + 1 + K, Base + 2 + K });
        else S.I.Append({ Base, Base + 2 + K, Base + 1 + K });
    }
}

/** Crumpled ball: jittered sphere with faceted normals. */
void HallCrumple(FHallMesh& M, FName Key, float R, int32 Seed)
{
    FRandomStream Rand(Seed);
    const int32 Rings = 7, Seg = 10;
    TArray<FVector> P;
    for (int32 Rg = 0; Rg <= Rings; ++Rg)
        for (int32 K = 0; K < Seg; ++K)
        {
            const float Phi = PI * Rg / Rings, Th = 2 * PI * K / Seg + (Rg % 2) * .3f;
            const float Rr = R * Rand.FRandRange(.72f, 1.12f);
            P.Add(FVector(FMath::Sin(Phi) * FMath::Cos(Th) * Rr, FMath::Sin(Phi) * FMath::Sin(Th) * Rr * .9f, FMath::Cos(Phi) * Rr * .85f));
        }
    FHallSection& S = M.Sec(Key);
    auto Tri = [&](const FVector& A, const FVector& B, const FVector& C)
    {
        FVector N = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
        if (FVector::DotProduct(N, (A + B + C) / 3.f) < 0) N = -N;
        const int32 Base = S.V.Num();
        S.V.Append({ A, B, C }); S.N.Append({ N, N, N });
        S.UV.Append({ FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1) });
        S.T.Append({ FProcMeshTangent((B - A).GetSafeNormal(), false), FProcMeshTangent((B - A).GetSafeNormal(), false), FProcMeshTangent((B - A).GetSafeNormal(), false) });
        if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), N) < 0) S.I.Append({ Base, Base + 1, Base + 2 });
        else S.I.Append({ Base, Base + 2, Base + 1 });
    };
    for (int32 Rg = 0; Rg < Rings; ++Rg)
        for (int32 K = 0; K < Seg; ++K)
        {
            const FVector A = P[Rg * Seg + K], B = P[Rg * Seg + (K + 1) % Seg], C = P[(Rg + 1) * Seg + K], D = P[(Rg + 1) * Seg + (K + 1) % Seg];
            Tri(A, B, D); Tri(A, D, C);
        }
}
}

AHouseBlackHallLitter::AHouseBlackHallLitter()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicatingMovement(true);
    NetDormancy = DORM_Initial;
    Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Litter"));
    RootComponent = Mesh;
    Mesh->bUseComplexAsSimpleCollision = false;
    Mesh->bUseAsyncCooking = false;
    Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
    Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    Mesh->SetGenerateOverlapEvents(true);
    Mesh->CanCharacterStepUpOn = ECB_No;
    Mesh->SetCanEverAffectNavigation(false);
    Mesh->BodyInstance.bUseCCD = true;
}

void AHouseBlackHallLitter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(AHouseBlackHallLitter, Kind, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(AHouseBlackHallLitter, Variant, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(AHouseBlackHallLitter, Seed, COND_InitialOnly);
}

float AHouseBlackHallLitter::RestHalfHeight(uint8 InKind, bool bStanding)
{
    switch (InKind)
    {
    case Can: return bStanding ? 6.4f : 3.4f;
    case CrushedCan: return 2.4f;
    case Cup: return bStanding ? 6.1f : 4.4f;
    case Paper: return 3.6f;
    case Bottle: return bStanding ? 9.f : 3.4f;
    case Plate: return .8f;
    case ChipBag: return 1.4f;
    case PizzaBox: return 2.3f;
    case Keg: return .5f;
    default: return 4.f;
    }
}

void AHouseBlackHallLitter::BeginPlay()
{
    Super::BeginPlay();
    BuildItem();
    if (Kind == Keg) SetNetDormancy(DORM_Awake);
    else if (HasAuthority()) Mesh->OnComponentBeginOverlap.AddDynamic(this, &AHouseBlackHallLitter::OnTouched);
}

void AHouseBlackHallLitter::BuildItem()
{
    UHouseBlackHallSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UHouseBlackHallSubsystem>() : nullptr;
    if (!Sub || Kind >= KindCount) return;
    FRandomStream R(Seed);
    FHallMesh M;
    const FHallUV Plain = FHallUV::World(10.f);
    switch (Kind)
    {
    case Can:
    case CrushedCan:
    {
        const FName Label(*FString::Printf(TEXT("CanLabel%d"), Variant % 5));
        M.Cylinder(Label, FVector(0, 0, 5.4f), FVector(0, 0, -5.4f), 3.3f, 18, FHallUV::World(10.8f), false);
        HallFrustum(M, TEXT("CanTop"), FVector(0, 0, 5.4f), FVector(0, 0, 6.2f), 3.3f, 2.75f, 18, false);
        HallFrustum(M, TEXT("CanTop"), FVector(0, 0, 6.2f), FVector(0, 0, 6.45f), 2.75f, 2.75f, 18, false);
        HallDisc(M, TEXT("CanTop"), FVector(0, 0, 6.2f), FVector::UpVector, 2.6f, 18);
        HallFrustum(M, TEXT("CanTop"), FVector(0, 0, -5.4f), FVector(0, 0, -6.1f), 3.3f, 2.6f, 18, false);
        HallDisc(M, TEXT("CanTop"), FVector(0, 0, -6.1f), -FVector::UpVector, 2.6f, 18);
        M.BoxMinMax(TEXT("CanTop"), FVector(-.4f, -.6f, 6.2f), FVector(1.6f, .6f, 6.35f), Plain);
        if (Kind == CrushedCan)
        {
            // Stomped flat with dents.
            for (auto& Pair : M.Sections)
                for (int32 I = 0; I < Pair.Value.V.Num(); ++I)
                {
                    FVector& V = Pair.Value.V[I];
                    const float Dent = HallNoise(V.X * .6f + Seed, V.Z * .5f, 13) - .5f;
                    V = FVector(V.X * 1.22f + Dent * 1.2f, V.Y * .62f + Dent * .8f, V.Z * (.42f + .08f * FMath::Sin(V.X)));
                    FVector& N = Pair.Value.N[I];
                    N = FVector(N.X / 1.22f, N.Y / .62f, N.Z / .42f).GetSafeNormal();
                }
        }
        break;
    }
    case Cup:
    {
        HallFrustum(M, TEXT("CupRed"), FVector(0, 0, -6.f), FVector(0, 0, 6.f), 3.f, 4.7f, 20, false);
        HallFrustum(M, TEXT("CupInside"), FVector(0, 0, -5.6f), FVector(0, 0, 5.95f), 2.85f, 4.55f, 20, true);
        HallDisc(M, TEXT("CupRed"), FVector(0, 0, -6.f), -FVector::UpVector, 3.f, 20);
        HallDisc(M, TEXT("CupInside"), FVector(0, 0, -5.6f), FVector::UpVector, 2.85f, 20);
        HallFrustum(M, TEXT("CupInside"), FVector(0, 0, 5.95f), FVector(0, 0, 6.2f), 4.62f, 4.78f, 20, false);
        for (const float Z : { 1.2f, -2.f }) HallFrustum(M, TEXT("CupRed"), FVector(0, 0, Z), FVector(0, 0, Z + .35f), 3.95f + Z * .07f, 3.98f + Z * .07f, 20, false);
        break;
    }
    case Paper:
        HallCrumple(M, R.FRand() < .25f ? FName(TEXT("PaperStained")) : FName(TEXT("PaperWhite")), R.FRandRange(3.2f, 4.6f), Seed);
        break;
    case Bottle:
    {
        M.Cylinder(TEXT("BottleClear"), FVector(0, 0, 4.5f), FVector(0, 0, -8.5f), 3.2f, 16, Plain, false);
        HallDisc(M, TEXT("BottleClear"), FVector(0, 0, -8.5f), -FVector::UpVector, 3.2f, 16);
        HallFrustum(M, TEXT("BottleClear"), FVector(0, 0, 4.5f), FVector(0, 0, 8.2f), 3.2f, 1.3f, 16, false);
        M.Cylinder(TEXT("BottleClear"), FVector(0, 0, 8.2f), FVector(0, 0, 9.5f), 1.3f, 12, Plain, false);
        M.Cylinder(TEXT("CapBlue"), FVector(0, 0, 9.5f), FVector(0, 0, 10.8f), 1.55f, 12, Plain, true);
        M.Cylinder(TEXT("LabelBlue"), FVector(0, 0, 1.5f), FVector(0, 0, -3.5f), 3.26f, 16, Plain, false);
        break;
    }
    case Plate:
        HallFrustum(M, TEXT("PaperWhite"), FVector(0, 0, -.4f), FVector(0, 0, .7f), 8.f, 11.f, 24, false);
        HallFrustum(M, TEXT("PaperWhite"), FVector(0, 0, -.25f), FVector(0, 0, .75f), 7.9f, 10.9f, 24, true);
        HallDisc(M, TEXT("PaperWhite"), FVector(0, 0, -.4f), -FVector::UpVector, 8.f, 24);
        HallDisc(M, R.FRand() < .5f ? FName(TEXT("PaperStained")) : FName(TEXT("PaperWhite")), FVector(0, 0, -.25f), FVector::UpVector, 7.9f, 24);
        break;
    case ChipBag:
    {
        const FRotator Twist(0, 0, R.FRandRange(-6.f, 6.f));
        M.Box(TEXT("FoilOrange"), FTransform(Twist, FVector::ZeroVector), FVector(7.5f, 10.f, 1.2f), Plain);
        M.Box(TEXT("FoilSilver"), FTransform(Twist, FVector(0, 10.6f, 0)), FVector(7.5f, .7f, .25f), Plain);
        M.Box(TEXT("FoilSilver"), FTransform(Twist, FVector(0, -10.6f, 0)), FVector(7.5f, .7f, .25f), Plain);
        M.Box(TEXT("FoilSilver"), FTransform(Twist, FVector(0, 2.f, 1.25f)), FVector(4.f, 3.f, .05f), Plain);
        break;
    }
    case PizzaBox:
        M.BoxMinMax(TEXT("Cardboard"), FVector(-19.f, -19.f, -2.2f), FVector(19.f, 19.f, 2.2f), Plain);
        M.BoxMinMax(TEXT("CardboardDark"), FVector(-19.1f, -15.f, .8f), FVector(-18.95f, 15.f, 1.f), Plain);
        M.BoxMinMax(TEXT("PaperStained"), FVector(-6.f, -4.f, 2.2f), FVector(5.f, 6.f, 2.23f), Plain);
        break;
    case Keg:
    {
        // Ice tub.
        HallFrustum(M, TEXT("TubBlue"), FVector(0, 0, 0), FVector(0, 0, 36.f), 26.f, 29.f, 32, false);
        HallFrustum(M, TEXT("TubBlue"), FVector(0, 0, 1.f), FVector(0, 0, 36.f), 25.4f, 28.4f, 32, true);
        HallDisc(M, TEXT("TubBlue"), FVector(0, 0, 0), -FVector::UpVector, 26.f, 32);
        HallFrustum(M, TEXT("TubBlue"), FVector(0, 0, 35.5f), FVector(0, 0, 37.5f), 29.8f, 29.8f, 32, false);
        HallFrustum(M, TEXT("TubBlue"), FVector(0, 0, 35.5f), FVector(0, 0, 37.5f), 28.3f, 28.3f, 32, true);
        HallFrustum(M, TEXT("TubBlue"), FVector(0, 0, 37.5f), FVector(0, 0, 37.501f), 29.8f, 28.3f, 32, false);
        for (const float A : { 0.f, 180.f })
        {
            const FVector Dir(FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A)), 0);
            M.Box(TEXT("TubBlue"), FTransform(Dir.Rotation(), Dir * 30.6f + FVector(0, 0, 33.f)), FVector(1.2f, 9.f, 1.6f), Plain);
        }
        // Ice around the keg.
        HallDisc(M, TEXT("Ice"), FVector(0, 0, 30.f), FVector::UpVector, 28.f, 32);
        for (int32 K = 0; K < 70; ++K)
        {
            const float Ang = R.FRandRange(0.f, 2.f * PI), Rad = R.FRandRange(21.5f, 26.8f), Sz = R.FRandRange(1.2f, 2.1f);
            M.Box(TEXT("Ice"), FTransform(FRotator(R.FRandRange(0.f, 360.f), R.FRandRange(0.f, 360.f), R.FRandRange(0.f, 360.f)),
                FVector(FMath::Cos(Ang) * Rad, FMath::Sin(Ang) * Rad, 30.5f + R.FRandRange(0.f, 2.5f))), FVector(Sz, Sz * R.FRandRange(.8f, 1.2f), Sz * R.FRandRange(.8f, 1.2f)), Plain);
        }
        // Half-barrel keg (40.6 cm x 58.4 cm) standing in the tub.
        const float KB = 6.f, KT = 64.4f;
        M.Cylinder(TEXT("Steel"), FVector(0, 0, KT - 9.f), FVector(0, 0, KB + 9.f), 20.3f, 32, FHallUV::World(40.f), false);
        M.Cylinder(TEXT("Steel"), FVector(0, 0, KB + 9.f), FVector(0, 0, KB), 20.f, 32, FHallUV::World(40.f), false);
        HallFrustum(M, TEXT("Steel"), FVector(0, 0, KT - 9.f), FVector(0, 0, KT), 20.f, 20.f, 32, false);
        HallFrustum(M, TEXT("Steel"), FVector(0, 0, KT - 9.f), FVector(0, 0, KT), 19.4f, 19.4f, 32, true);
        HallFrustum(M, TEXT("Steel"), FVector(0, 0, KT), FVector(0, 0, KT + .001f), 20.f, 19.4f, 32, false);
        for (const float Z : { KB + 14.6f, KB + 43.8f })
        {
            HallFrustum(M, TEXT("Steel"), FVector(0, 0, Z - 1.3f), FVector(0, 0, Z), 20.3f, 21.f, 32, false);
            HallFrustum(M, TEXT("Steel"), FVector(0, 0, Z), FVector(0, 0, Z + 1.3f), 21.f, 20.3f, 32, false);
        }
        HallDisc(M, TEXT("Steel"), FVector(0, 0, KT - 6.5f), FVector::UpVector, 19.4f, 32);
        for (const float A : { 90.f, 270.f })
        {
            const FVector Dir(FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A)), 0);
            M.Box(TEXT("HoleDark"), FTransform(Dir.Rotation(), Dir * 20.05f + FVector(0, 0, KT - 3.6f)), FVector(.1f, 6.f, 2.f), Plain);
        }
        // Sankey valve, coupler and party pump.
        M.Cylinder(TEXT("Steel"), FVector(0, 0, KT - 6.5f), FVector(0, 0, KT - 4.f), 2.7f, 16, Plain, true);
        M.Cylinder(TEXT("PumpBlack"), FVector(0, 0, KT - 4.f), FVector(0, 0, KT + 2.f), 3.8f, 16, Plain, true);
        M.Box(TEXT("PumpBlack"), FTransform(FRotator(12.f, 200.f, 0), FVector(-6.f, -2.2f, KT + 1.f)), FVector(5.f, .9f, .7f), Plain);
        M.Cylinder(TEXT("PumpBlack"), FVector(0, 0, KT + 2.f), FVector(0, 0, KT + 22.f), 2.6f, 16, Plain, true);
        M.Cylinder(TEXT("Chrome"), FVector(0, 0, KT + 22.f), FVector(0, 0, KT + 25.f), .7f, 10, Plain, true);
        M.Box(TEXT("PumpBlack"), FTransform(FRotator(0, 30.f, 0), FVector(0, 0, KT + 26.f)), FVector(7.f, 1.2f, 1.2f), Plain);
        // Beer line draped over the chime to a picnic tap.
        const FVector Hose[] = { FVector(3.6f, 0, KT - 1.f), FVector(11.f, .5f, KT + 2.5f), FVector(18.f, 1.f, KT + 2.f), FVector(21.5f, 1.5f, KT - 2.f),
            FVector(22.6f, 2.5f, KT - 10.f), FVector(23.f, 3.5f, KT - 18.f), FVector(23.3f, 4.5f, KT - 24.f) };
        for (int32 K = 0; K + 1 < (int32)UE_ARRAY_COUNT(Hose); ++K) M.Cylinder(TEXT("HoseClear"), Hose[K], Hose[K + 1], .65f, 8, Plain, true);
        const FVector TapAt = Hose[UE_ARRAY_COUNT(Hose) - 1];
        M.Box(TEXT("PumpBlack"), FTransform(FRotator(0, 20.f, 0), TapAt + FVector(0, 0, -4.f)), FVector(1.6f, 1.6f, 4.5f), Plain);
        M.Box(TEXT("PumpBlack"), FTransform(FRotator(0, 20.f, 0), TapAt + FVector(1.4f, .5f, -1.f)), FVector(2.6f, .5f, .9f), Plain);
        M.Cylinder(TEXT("PumpBlack"), TapAt + FVector(0, 0, -8.5f), TapAt + FVector(0, 0, -10.f), .9f, 8, Plain, true);
        break;
    }
    default: break;
    }
    // Physics: convex hull of the visible geometry.
    TArray<FVector> Hull;
    if (Kind == Keg)
    {
        // Tub and barrel only (the pump and hose are not part of the solid).
        for (int32 K = 0; K < 24; ++K)
        {
            const float A = 2 * PI * K / 24;
            const FVector D(FMath::Cos(A), FMath::Sin(A), 0);
            Hull.Append({ D * 26.f, D * 29.8f + FVector(0, 0, 37.5f), D * 20.3f + FVector(0, 0, 64.4f) });
        }
    }
    else
        for (const auto& Pair : M.Sections) for (int32 I = 0; I < Pair.Value.V.Num(); I += (Pair.Value.V.Num() > 200 ? 2 : 1)) Hull.Add(Pair.Value.V[I]);
    M.Build(Mesh, [Sub](FName Key) { return Sub->Mat(Key); }, false);
    Mesh->ClearCollisionConvexMeshes();
    Mesh->AddCollisionConvexMesh(Hull);
    const FHallLitterPhys& P = HallLitterPhysTable[Kind];
    if (UPhysicalMaterial* PM = Sub->LitterPhysics(Kind)) Mesh->SetPhysMaterialOverride(PM);
    Mesh->SetLinearDamping(P.LinDamp);
    Mesh->SetAngularDamping(P.AngDamp);
    Mesh->SetMassOverrideInKg(NAME_None, P.Mass, true);
    if (Kind == Keg)
    {
        // Heavy and solid: players shove it a little but cannot walk through or tip it.
        Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Mesh->SetGenerateOverlapEvents(false);
        Mesh->BodyInstance.bLockXRotation = true;
        Mesh->BodyInstance.bLockYRotation = true;
        Mesh->BodyInstance.SetDOFLock(EDOFMode::SixDOF);
    }
    Mesh->SetSimulatePhysics(true);
}

void AHouseBlackHallLitter::OnTouched(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (Cast<ACharacter>(OtherActor)) Kick(OtherActor);
}

void AHouseBlackHallLitter::Kick(const AActor* By)
{
    if (!HasAuthority() || !By || Kind >= KindCount || Kind == Keg) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastKick < .3) return;
    LastKick = Now;
    FVector Vel = By->GetVelocity(); Vel.Z = 0;
    const float Speed = Vel.Size();
    FVector Away = GetActorLocation() - By->GetActorLocation(); Away.Z = 0;
    Away = Away.GetSafeNormal();
    const FVector Forward = Speed > 30.f ? Vel / Speed : Away;
    const FVector Dir = (Forward * .8f + Away * .4f).GetSafeNormal2D();
    const FHallLitterPhys& P = HallLitterPhysTable[Kind];
    FRandomStream R(Seed + int32(Now * 100));
    // A walking foot taps things along; a sprint sends a can skittering a few metres.
    const float Out = FMath::Clamp(Speed * .5f + 35.f, 35.f, 380.f) * P.KickScale * R.FRandRange(.75f, 1.1f);
    const float Up = FMath::Clamp(Speed * .07f, 8.f, 60.f) * (P.KickScale > .7f ? 1.f : .4f) * R.FRandRange(.5f, 1.2f);
    SetNetDormancy(DORM_Awake);
    Mesh->WakeAllRigidBodies();
    Mesh->AddImpulse(Dir * Out + FVector(0, 0, Up), NAME_None, true);
    Mesh->AddAngularImpulseInDegrees(R.VRand() * R.FRandRange(150.f, 520.f) * P.KickScale, NAME_None, true);
}

// =============================================================================================
UPhysicalMaterial* UHouseBlackHallSubsystem::LitterPhysics(uint8 InKind)
{
    if (InKind >= AHouseBlackHallLitter::KindCount) return nullptr;
    const FName Name(*FString::Printf(TEXT("PM_BH_Litter%d"), InKind));
    for (UObject* Obj : KeepAlive) if (Obj && Obj->GetFName() == Name) return Cast<UPhysicalMaterial>(Obj);
    UPhysicalMaterial* PM = NewObject<UPhysicalMaterial>(this, Name);
    PM->Friction = HallLitterPhysTable[InKind].Friction;
    PM->Restitution = HallLitterPhysTable[InKind].Restitution;
    KeepAlive.Add(PM);
    return PM;
}

void UHouseBlackHallSubsystem::SpawnLitter()
{
    UWorld* World = GetWorld();
    FRandomStream R(20261005);
    struct FZone { FBox2D Area; int32 Count; float Z; };
    const FZone Zones[] = {
        { FBox2D(FVector2D(-1212, -260), FVector2D(-1040, 40)), 12, ZF },      // around the west trash can
        { FBox2D(FVector2D(-780, -462), FVector2D(740, -395)), 18, ZF },       // along the stage front
        { FBox2D(FVector2D(-560, -770), FVector2D(560, -500)), 7, -299.f },    // up on the stage
        { FBox2D(FVector2D(-1212, -780), FVector2D(-940, -480)), 9, ZF },      // south-west corner
        { FBox2D(FVector2D(-1100, -380), FVector2D(1100, 560)), 38, ZF },      // scattered over the floor
        { FBox2D(FVector2D(-1212, 690), FVector2D(1212, 782)), 12, ZF },       // along the patio-door wall
        { FBox2D(FVector2D(1230, 160), FVector2D(1294, 360)), 4, ZF },         // XXVII alcove
        { FBox2D(FVector2D(1110, 290), FVector2D(1214, 420)), 4, ZF },         // by the exit door
        { FBox2D(FVector2D(1000, -780), FVector2D(1212, -560)), 7, ZF },       // south-east corner
        { FBox2D(FVector2D(880, -340), FVector2D(1210, 0)), 6, ZF },           // east wall
        { FBox2D(FVector2D(690, 330), FVector2D(980, 650)), 14, ZF },          // party pile round the folding table
        { FBox2D(FVector2D(-920, -470), FVector2D(-680, -280)), 9, ZF },       // pile at the stage's west end
    };
    // Kind weights: cans dominate, then cups, paper...
    const float Weights[AHouseBlackHallLitter::KindCount] = { .34f, .10f, .18f, .16f, .06f, .06f, .06f, .04f };
    int32 PizzaBoxes = 0;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(HallLitterPlace), false);
    {
        // The keg goes first (beside the folding table) so the scattered trash settles around it.
        const FTransform T = HouseWorldScale::Transform(World, FTransform(FRotator(0, 35.f, 0),
            FVector(640.f, 455.f, ZF + AHouseBlackHallLitter::RestHalfHeight(AHouseBlackHallLitter::Keg, true))));
        if (AHouseBlackHallLitter* KegItem = World->SpawnActorDeferred<AHouseBlackHallLitter>(AHouseBlackHallLitter::StaticClass(), T, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
        {
            KegItem->Kind = AHouseBlackHallLitter::Keg; KegItem->Seed = 4242;
            KegItem->FinishSpawning(T);
            Litter.Add(KegItem);
        }
    }
    for (const FZone& Zone : Zones)
        for (int32 N = 0; N < Zone.Count; ++N)
        {
            float Pick = R.FRand(); uint8 K = 0;
            for (; K < AHouseBlackHallLitter::KindCount - 1; ++K) { if (Pick < Weights[K]) break; Pick -= Weights[K]; }
            if (K >= AHouseBlackHallLitter::Keg) K = AHouseBlackHallLitter::Can;
            if (K == AHouseBlackHallLitter::PizzaBox && (++PizzaBoxes > 2 || Zone.Z != ZF)) K = AHouseBlackHallLitter::Cup;
            if (Zone.Z != ZF && K != AHouseBlackHallLitter::Cup && K != AHouseBlackHallLitter::Can) K = AHouseBlackHallLitter::Can;
            const bool bStanding = (K == AHouseBlackHallLitter::Can && R.FRand() < .22f) || (K == AHouseBlackHallLitter::Cup && R.FRand() < .15f)
                || (K == AHouseBlackHallLitter::Bottle && R.FRand() < .25f);
            const float Half = AHouseBlackHallLitter::RestHalfHeight(K, bStanding);
            for (int32 Attempt = 0; Attempt < 14; ++Attempt)
            {
                const FVector2D P(R.FRandRange(Zone.Area.Min.X, Zone.Area.Max.X), R.FRandRange(Zone.Area.Min.Y, Zone.Area.Max.Y));
                // Keep the swing of every patio pair and the exit landing clear.
                bool bBlocked = false;
                for (const HallLayout::FPairSpec& Pair : HallLayout::Pairs)
                    bBlocked |= FMath::Abs(P.X - Pair.Center) < Pair.FrameWidth * .5f + 12.f && P.Y > YN - Pair.FrameWidth * .5f - 20.f;
                if (bBlocked) continue;
                const FVector Loc(P.X, P.Y, Zone.Z + Half + 1.5f);
                const float TestR = FMath::Max(6.f, Half + 1.f);
                if (World->OverlapBlockingTestByChannel(HouseWorldScale::Point(World, FVector(P.X, P.Y, Zone.Z + TestR + 1.f)),
                    FQuat::Identity, ECC_PhysicsBody, FCollisionShape::MakeSphere(TestR * HouseWorldScale::Factor(World)), Q)) continue;
                FRotator Rot(0, R.FRandRange(0.f, 360.f), 0);
                const bool bRolls = K == AHouseBlackHallLitter::Can || K == AHouseBlackHallLitter::Cup || K == AHouseBlackHallLitter::Bottle;
                if (bRolls && !bStanding) Rot = FRotator(90.f, R.FRandRange(0.f, 360.f), 0);
                if (K == AHouseBlackHallLitter::Paper || K == AHouseBlackHallLitter::CrushedCan) Rot = FRotator(R.FRandRange(-25.f, 25.f), R.FRandRange(0.f, 360.f), R.FRandRange(-25.f, 25.f));
                const FTransform T = HouseWorldScale::Transform(World, FTransform(Rot, Loc));
                AHouseBlackHallLitter* Item = World->SpawnActorDeferred<AHouseBlackHallLitter>(AHouseBlackHallLitter::StaticClass(), T, nullptr, nullptr,
                    ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
                if (!Item) break;
                Item->Kind = K; Item->Variant = uint8(R.RandRange(0, 4)); Item->Seed = R.RandRange(1, 1 << 20);
                Item->FinishSpawning(T);
                Litter.Add(Item);
                break;
            }
        }
}
