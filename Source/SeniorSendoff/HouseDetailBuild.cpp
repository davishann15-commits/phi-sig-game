// Hallway dressing: fixtures, signs, conduit, prints, wall damage and ceiling items placed against traced walls.
#include "HouseDetail.h"
#include "HouseExitSign.h"
#include "ExistingHouseDoor.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseReactiveFixture.h"
#include "HouseWorldScale.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace
{
/** A point on a wall face at floor level: U runs to the right as seen from the hall, N points into the hall. */
struct FHwAnchor
{
    FVector O = FVector::ZeroVector, U = FVector(0, 1, 0), N = FVector(-1, 0, 0);
    float CeilZ = 0.f;
    FLinearColor Wall = FLinearColor(.5f, .5f, .5f); float WallRough = .7f; bool bWallPlain = true;
    FVector P(float Along, float Depth, float Z) const { return O + U * Along + N * Depth + FVector(0, 0, Z); }
    /** Box face bit (FHallMesh skip mask) of the face that looks into the hall. */
    int32 FrontBit() const
    {
        if (N.X > .5f) return 1 << 0;
        if (N.X < -.5f) return 1 << 1;
        if (N.Y > .5f) return 1 << 2;
        return 1 << 3;
    }
};
struct FHwCorridor
{
    const TCHAR* Name;
    FVector A, B;           // spine at eye height
    FVector Side;           // "+" side direction
    float PlusDist, MinusDist;  // expected wall distances (0 = no wall)
};

bool HwIgnored(const AActor* Actor)
{
    return Actor && (Actor->IsA<AExistingHouseDoor>() || Actor->IsA<AHouseInteractiveProp>() || Actor->IsA<AHouseReactiveFixture>() || Actor->IsA<AHouseHallwayDetail>());
}

/** First structural hit along a ray (doors, furniture and fixtures are skipped). */
bool HwTrace(UWorld* World, const FVector& From, const FVector& Dir, float MaxDist, FHitResult& Out)
{
    TArray<FHitResult> Hits;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(HwTrace), true);
    Q.bReturnFaceIndex = true;
    // The room measurements and generated vertices are in original authoring
    // space. Trace the resized live shell, then bring hit positions/distances
    // back into that same authoring space before fitting the fixtures.
    if (!World->LineTraceMultiByChannel(Hits, HouseWorldScale::Point(World, From),
        HouseWorldScale::Point(World, From + Dir * MaxDist), ECC_Visibility, Q)) return false;
    const float Scale = HouseWorldScale::Factor(World);
    for (FHitResult& H : Hits)
    {
        H.Location = HouseWorldScale::Unpoint(World, H.Location);
        H.ImpactPoint = HouseWorldScale::Unpoint(World, H.ImpactPoint);
        H.TraceStart = HouseWorldScale::Unpoint(World, H.TraceStart);
        H.TraceEnd = HouseWorldScale::Unpoint(World, H.TraceEnd);
        H.Distance /= Scale;
    }
    for (const FHitResult& H : Hits)
        if (H.bBlockingHit && !HwIgnored(H.GetActor())) { Out = H; return true; }
    // A blocking hit on an ignored actor stops the multi trace; probe again past it.
    for (const FHitResult& H : Hits)
        if (H.bBlockingHit && HwIgnored(H.GetActor()))
        {
            FHitResult Again;
            const float Left = MaxDist - H.Distance - 2.f;
            if (Left > 10.f && HwTrace(World, H.ImpactPoint + Dir * 2.f, Dir, Left, Again)) { Out = Again; Out.Distance += H.Distance + 2.f; return true; }
            return false;
        }
    return false;
}

/** Material tint of the wall where a trace hit (plain painted walls only). */
void HwReadWall(const FHitResult& Hit, FHwAnchor& A)
{
    A.bWallPlain = false;
    UPrimitiveComponent* C = Hit.GetComponent();
    if (!C) return;
    FString Name = C->GetOwner() ? C->GetOwner()->GetActorNameOrLabel() : FString();
    UMaterialInterface* Mat = nullptr;
    auto IsWallPaint = [](const FString& M) { return M.Contains(TEXT("HallGrey")) || M.Contains(TEXT("GalleryWallGrey")) || M.Contains(TEXT("HallPaint")) || M.Contains(TEXT("WallGrey")); };
    if (Hit.FaceIndex >= 0)
    {
        int32 Section = 0;
        if (UMaterialInterface* Direct = C->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex, Section))
            if (IsWallPaint(GetNameSafe(Direct))) Mat = Direct;
    }
    if (!Mat && Name.StartsWith(TEXT("SM_")))
        for (int32 K = 0; K < C->GetNumMaterials() && !Mat; ++K)
            if (IsWallPaint(GetNameSafe(C->GetMaterial(K)))) Mat = C->GetMaterial(K);
    // Collision tiles are invisible; look for the visible tile of the same cell to read the paint.
    if (!Mat && (Name.StartsWith(TEXT("COL_")) || Name.StartsWith(TEXT("COLF_"))))
    {
        const FString Visible = TEXT("SM_") + Name.RightChop(Name.StartsWith(TEXT("COLF_")) ? 5 : 4);
        for (TActorIterator<AStaticMeshActor> It(C->GetWorld()); It; ++It)
            if (It->GetActorNameOrLabel() == Visible && It->GetStaticMeshComponent())
            {
                UStaticMeshComponent* S = It->GetStaticMeshComponent();
                for (int32 K = 0; K < S->GetNumMaterials() && !Mat; ++K)
                    if (IsWallPaint(GetNameSafe(S->GetMaterial(K)))) Mat = S->GetMaterial(K);
                break;
            }
    }
    if (!Mat) return;
    FLinearColor Tint; float Rough = .7f; UTexture* Base = nullptr;
    if (!Mat->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Tint")), Tint)) return;
    Mat->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Roughness")), Rough);
    Mat->GetTextureParameterValue(FMaterialParameterInfo(TEXT("BaseMap")), Base);
    const FString BaseName = GetNameSafe(Base);
    A.bWallPlain = !Base || BaseName.Contains(TEXT("WhiteSquare")) || BaseName.Contains(TEXT("Default"));
    A.Wall = Tint; A.WallRough = Rough;
}

class FHwBuilder
{
public:
    FHwBuilder(UWorld* InWorld, UHouseBlackHallSubsystem* InLook, AHouseHallwayDetail* InOwner) : World(InWorld), Look(InLook), Owner(InOwner) {}
    UWorld* World; UHouseBlackHallSubsystem* Look; AHouseHallwayDetail* Owner;
    FHallMesh Decor;      // no collision
    FHallMesh Bulky;      // cabinets and panels players can bump
    const FHallUV Plain = FHallUV::World(50.f);
    FRandomStream Rng{ 20261007 };
    int32 Plaque = 0;

    /** Anchor on the wall of Corridor at distance T along the spine, Side +1 / -1. */
    bool Anchor(const FHwCorridor& C, float Side, float T, FHwAnchor& Out, float Tolerance = 25.f)
    {
        const FVector Dir = (C.B - C.A).GetSafeNormal();
        const FVector Eye = C.A + Dir * T;
        const FVector Toward = C.Side * Side;
        const float Expected = Side > 0 ? C.PlusDist : C.MinusDist;
        if (Expected <= 0.f) { ++Owner->Skipped; return false; }
        FHitResult Hit;
        if (!HwTrace(World, Eye, Toward, Expected + Tolerance + 60.f, Hit) || FMath::Abs(Hit.Distance - Expected) > Tolerance)
        {
            Owner->Notes += FString::Printf(TEXT(" miss:%s@%.0f/%d"), C.Name, T, Side > 0 ? 1 : -1);
            ++Owner->Skipped; return false;
        }
        FVector N = -Toward;
        if (FMath::Abs(Hit.ImpactNormal.X) > .7f || FMath::Abs(Hit.ImpactNormal.Y) > .7f) N = FVector(FMath::RoundToFloat(Hit.ImpactNormal.X), FMath::RoundToFloat(Hit.ImpactNormal.Y), 0).GetSafeNormal();
        Out.N = N; Out.U = FVector::CrossProduct(N, FVector::UpVector).GetSafeNormal();
        // Floor and ceiling directly in front of the hit.
        const FVector Probe = Hit.ImpactPoint + N * 20.f;
        FHitResult Floor, Ceil;
        float FloorZ = Eye.Z - 118.f, CeilZ = Eye.Z + 145.f;
        if (HwTrace(World, Probe, FVector(0, 0, -1), 300.f, Floor)) FloorZ = Floor.ImpactPoint.Z;
        if (HwTrace(World, Probe, FVector(0, 0, 1), 300.f, Ceil)) CeilZ = Ceil.ImpactPoint.Z;
        Out.O = FVector(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, FloorZ);
        Out.CeilZ = CeilZ;
        HwReadWall(Hit, Out);
        ++Owner->Placed;
        return true;
    }
    /** Ceiling point above the spine at T (0 = centre, Offset across the hall). */
    bool Ceiling(const FHwCorridor& C, float T, float Across, FVector& Out, float& FloorZ)
    {
        const FVector Dir = (C.B - C.A).GetSafeNormal();
        const FVector Eye = C.A + Dir * T + C.Side * Across;
        FHitResult Hit, Floor;
        if (!HwTrace(World, Eye, FVector(0, 0, 1), 300.f, Hit)) { ++Owner->Skipped; return false; }
        Out = Hit.ImpactPoint; FloorZ = Eye.Z - 118.f;
        if (HwTrace(World, Eye, FVector(0, 0, -1), 300.f, Floor)) FloorZ = Floor.ImpactPoint.Z;
        ++Owner->Placed;
        return true;
    }
    bool FloorPoint(const FVector& Near, FVector& Out)
    {
        FHitResult Hit;
        if (!HwTrace(World, Near + FVector(0, 0, 100), FVector(0, 0, -1), 300.f, Hit)) return false;
        Out = Hit.ImpactPoint; return true;
    }

    // ---------------------------------------------------------------- primitives in an anchor frame
    void ABox(FHallMesh& M, FName Key, const FHwAnchor& A, float U0, float U1, float D0, float D1, float Z0, float Z1, const FHallUV& UV, int32 Skip = 0)
    {
        const FVector P0 = A.P(U0, D0, Z0), P1 = A.P(U1, D1, Z1);
        M.BoxMinMax(Key, P0.ComponentMin(P1), P0.ComponentMax(P1), UV, Skip);
    }
    /** Painted quad on the wall plane (reads correctly from the hall). */
    void AFace(FHallMesh& M, FName Key, const FHwAnchor& A, float U0, float U1, float D, float Z0, float Z1)
    {
        M.QuadUV(Key, A.P(U0, D, Z1), A.P(U1, D, Z1), A.P(U1, D, Z0), A.P(U0, D, Z0), A.N, FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1), A.U);
    }
    void ACyl(FHallMesh& M, FName Key, const FHwAnchor& A, const FVector& P0, const FVector& P1, float R, int32 Seg, bool bCaps = true)
    {
        M.Cylinder(Key, P0, P1, R, Seg, Plain, bCaps);
    }

    // ---------------------------------------------------------------- fixtures
    /** Gadget frame: X out of the wall, Z up, origin on the wall at (U, Z). */
    FTransform Frame(const FHwAnchor& A, float U, float Z) const
    {
        return FTransform(FRotationMatrix::MakeFromXZ(A.N, FVector::UpVector).Rotator(), A.P(U, 0, Z));
    }
    void Gadget(const FHwAnchor& A, float U, float Z, EHouseGadgetKind Kind, uint8 Variant = 0)
    {
        FHouseGadgetSpec Spec;
        Spec.Kind = Kind; Spec.Frame = Frame(A, U, Z); Spec.Variant = Variant;
        Owner->GadgetSpecs.Add(Spec);
    }
    void PullStation(const FHwAnchor& A, float U, float Z)
    {
        ABox(Decor, TEXT("HW_Red"), A, U - 6.5f, U + 6.5f, 0, 6.5f, Z, Z + 14.f, Plain, A.FrontBit());
        AFace(Decor, TEXT("HW_FireFace"), A, U - 6.5f, U + 6.5f, 6.5f, Z, Z + 14.f);
        Gadget(A, U, Z, EHouseGadgetKind::PullStation);   // the handle is the gadget's moving part
    }
    void HornStrobe(const FHwAnchor& A, float U, float Z)
    {
        ABox(Decor, TEXT("HW_Red"), A, U - 6.5f, U + 6.5f, 0, 7.f, Z, Z + 15.f, Plain, A.FrontBit());
        AFace(Decor, TEXT("HW_HornFace"), A, U - 6.5f, U + 6.5f, 7.f, Z, Z + 15.f);
        Gadget(A, U, Z, EHouseGadgetKind::HornStrobe);    // the strobe lens flashes on the gadget
    }
    /** True when a plaque (about 40 x 38 cm incl. the hat) at (U, Z) has bare wall behind it: no fixtures, posters, props, doors or openings. */
    bool BassClear(const FHwAnchor& A, float U, float Z)
    {
        for (const FHallMesh* M : { &Decor, &Bulky })
            for (const auto& Pair : M->Sections)
                for (const FVector& V : Pair.Value.V)
                {
                    const FVector D = V - A.O;
                    const float Lu = FVector::DotProduct(D, A.U), Ln = FVector::DotProduct(D, A.N), Lz = D.Z;
                    if (FMath::Abs(Lu - U) < 23.f && Lz > Z - 16.f && Lz < Z + 23.f && Ln < 20.f && Ln > -5.f) return false;
                }
        for (const FHouseGadgetSpec& G : Owner->GadgetSpecs)
            if (FVector::Dist(G.Frame.GetLocation(), A.P(U, 0, Z)) < 40.f) return false;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(HouseBassClear), true);
        Q.AddIgnoredActor(Owner);
        for (int32 Iu = -1; Iu <= 1; ++Iu)
            for (int32 Iz = -1; Iz <= 1; ++Iz)
            {
                const FVector Start = A.P(U + Iu * 20.f, 40.f, Z + Iz * 13.f);
                FHitResult Hit;
                if (!World->LineTraceSingleByChannel(Hit, HouseWorldScale::Point(World, Start),
                    HouseWorldScale::Point(World, Start - A.N * 60.f), ECC_Visibility, Q)) return false;   // opening
                if (FMath::Abs(Hit.Distance / HouseWorldScale::Factor(World) - 40.f) > 2.5f) return false; // something in front, or a recess
            }
        return true;
    }
    void BillyBass(const FHwAnchor& A, float U, float Z)
    {
        Gadget(A, U, Z, EHouseGadgetKind::BillyBass);
    }
    void ExtinguisherCabinet(const FHwAnchor& A, float U, float Z)
    {
        const float W = 16.f, D = 13.f, H = 78.f;
        // Open-front box: back, sides, top, bottom.
        ABox(Bulky, TEXT("HW_White"), A, U - W, U + W, 0, 1.f, Z, Z + H, Plain);
        ABox(Bulky, TEXT("HW_White"), A, U - W, U - W + 1.5f, 0, D, Z, Z + H, Plain);
        ABox(Bulky, TEXT("HW_White"), A, U + W - 1.5f, U + W, 0, D, Z, Z + H, Plain);
        ABox(Bulky, TEXT("HW_White"), A, U - W, U + W, 0, D, Z, Z + 1.5f, Plain);
        ABox(Bulky, TEXT("HW_White"), A, U - W, U + W, 0, D, Z + H - 7.f, Z + H, Plain);
        AFace(Decor, TEXT("HW_CabinetStrip"), A, U - W + 1.f, U + W - 1.f, D + .05f, Z + H - 6.f, Z + H - 1.f);
        // Cavity shading.
        ABox(Decor, TEXT("HW_Cavity"), A, U - W + 1.5f, U + W - 1.5f, 1.f, 1.3f, Z + 1.5f, Z + H - 7.f, Plain);
        // Extinguisher: red cylinder, black neck and handle, hose.
        const FVector Base = A.P(U, 6.5f, Z + 4.f);
        ACyl(Decor, TEXT("HW_Red"), A, Base, Base + FVector(0, 0, 44.f), 7.f, 18);
        ACyl(Decor, TEXT("HW_Red"), A, Base + FVector(0, 0, 44.f), Base + FVector(0, 0, 47.f), 5.f, 14);
        ACyl(Decor, TEXT("HW_Black"), A, Base + FVector(0, 0, 47.f), Base + FVector(0, 0, 53.f), 2.6f, 12);
        ABox(Decor, TEXT("HW_Black"), A, U - 1.2f, U + 1.2f, 3.f, 10.f, Z + 55.f, Z + 57.f, Plain);
        ABox(Decor, TEXT("HW_Chrome"), A, U - 1.f, U + 1.f, 3.5f, 9.5f, Z + 57.f, Z + 58.6f, Plain);
        ABox(Decor, TEXT("HW_Black"), A, U - 1.f, U + 1.f, 9.f, 11.f, Z + 52.f, Z + 55.f, Plain);
        ACyl(Decor, TEXT("HW_Black"), A, A.P(U + 5.f, 6.5f, Z + 50.f), A.P(U + 10.5f, 6.5f, Z + 14.f), 1.1f, 8);
        AFace(Decor, TEXT("HW_ExtLabel"), A, U - 4.f, U + 4.f, 13.6f, Z + 16.f, Z + 34.f);
    }
    void FittedExit(const FVector& C, const FVector& N, int32 Arrow, bool bDouble)
    {
        if (!World->GetAuthGameMode()) return;
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        const FTransform Transform = HouseWorldScale::Transform(World, FTransform(
            FRotationMatrix::MakeFromXY(FVector::CrossProduct(N, FVector::UpVector), N).Rotator(), C));
        AHouseExitSign* Sign = World->SpawnActorDeferred<AHouseExitSign>(AHouseExitSign::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Sign) return;
        Sign->PropId = FName(*FString::Printf(TEXT("Exit_Hall_%.0f_%.0f"), C.X, C.Y));
        Sign->Configure(Arrow < 0 ? 1 : Arrow > 0 ? 2 : 0, bDouble);
        Sign->FinishSpawning(Transform);
    }
    void ExitSign(const FHwAnchor& A, float U, float Z, int32 Arrow)
    {
        FittedExit(A.P(U, 3.1f, Z + 10.f), A.N, Arrow, false);
    }
    void ExitPendant(const FVector& CeilPoint, const FVector& Along, const FVector& ExitDir, bool bArrow, float Drop = 35.f)
    {
        const FVector C = CeilPoint - FVector(0, 0, Drop);
        const FVector Across = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal();
        const int32 Arrow = bArrow ? (FVector::DotProduct(ExitDir, Across) > 0 ? 1 : -1) : 0;
        FittedExit(C, Along, Arrow, true);
        // A ceiling canopy and two short rods visibly support the housing.
        const FTransform Mount(FRotationMatrix::MakeFromXY(Across, Along).Rotator(), CeilPoint - FVector(0, 0, .6f));
        Decor.Box(TEXT("HW_Steel"), Mount, FVector(12.f, 4.5f, .6f), Plain);
        for (const float S : { -10.f, 10.f })
        {
            Decor.Cylinder(TEXT("HW_Steel"), C + Across*S + FVector(0,0,15.f), CeilPoint + Across*S - FVector(0,0,1.2f), .55f, 12, Plain);
            Decor.Cylinder(TEXT("HW_Steel"), C + Across*S + FVector(0,0,15.f), C + Across*S + FVector(0,0,15.8f), 1.1f, 12, Plain);
        }
    }
    void MailPlaque(const FHwAnchor& A, float U, float Z)
    {
        ABox(Decor, TEXT("HW_Pine"), A, U - 13.f, U + 13.f, 0, 1.6f, Z, Z + 8.f, FHallUV::World(40.f));
        AFace(Decor, FName(*FString::Printf(TEXT("HW_Plaque%d"), (Plaque++) % 4)), A, U - 11.4f, U + 11.4f, 1.7f, Z + 1.3f, Z + 6.7f);
    }
    void Thermostat(const FHwAnchor& A, float U, float Z)
    {
        ABox(Decor, TEXT("HW_White"), A, U - 6.5f, U + 6.5f, 0, 3.2f, Z, Z + 9.6f, Plain, A.FrontBit());
        AFace(Decor, TEXT("HW_Thermostat"), A, U - 6.5f, U + 6.5f, 3.25f, Z, Z + 9.6f);
    }
    void ElectricalPanel(const FHwAnchor& A, float U, float Z)
    {
        ABox(Bulky, TEXT("HW_PanelGrey"), A, U - 19.f, U + 19.f, 0, 9.f, Z, Z + 64.f, Plain, A.FrontBit());
        AFace(Decor, TEXT("HW_PanelFace"), A, U - 19.f, U + 19.f, 9.05f, Z, Z + 64.f);
        ABox(Decor, TEXT("HW_Black"), A, U + 13.f, U + 15.5f, 9.f, 10.2f, Z + 30.f, Z + 34.f, Plain);
    }
    void ConduitVertical(const FHwAnchor& A, float U, float Z0, float Z1, float JunctionZ)
    {
        ACyl(Decor, TEXT("HW_Steel"), A, A.P(U, 2.6f, Z0), A.P(U, 2.6f, Z1), 1.3f, 10, false);
        for (float Z = Z0 + 40.f; Z < Z1 - 20.f; Z += 90.f) ABox(Decor, TEXT("HW_Steel"), A, U - 2.4f, U + 2.4f, 0, 4.f, Z, Z + 1.8f, Plain);
        if (JunctionZ > 0)
        {
            ABox(Decor, TEXT("HW_PanelGrey"), A, U - 5.f, U + 5.f, 0, 5.4f, JunctionZ - 5.f, JunctionZ + 5.f, Plain);
            ABox(Decor, TEXT("HW_PanelGrey"), A, U - 5.4f, U + 5.4f, 5.4f, 5.9f, JunctionZ - 5.4f, JunctionZ + 5.4f, Plain);
        }
    }
    void ConduitHorizontal(const FHwAnchor& A, float U0, float U1, float Z)
    {
        ACyl(Decor, TEXT("HW_Steel"), A, A.P(U0, 2.6f, Z), A.P(U1, 2.6f, Z), 1.3f, 10, true);
        const float Lo = FMath::Min(U0, U1), Hi = FMath::Max(U0, U1);
        for (float U = Lo + 45.f; U < Hi - 20.f; U += 120.f) ABox(Decor, TEXT("HW_Steel"), A, U, U + 1.8f, 0, 4.f, Z - 2.4f, Z + 2.4f, Plain);
    }
    void SwitchPlate(const FHwAnchor& A, float U, float Z, int32 Gangs = 1)
    {
        const float HalfW = 3.6f + (Gangs - 1) * 2.3f;
        ABox(Decor, TEXT("HW_White"), A, U - HalfW, U + HalfW, 0, .6f, Z - 5.8f, Z + 5.8f, Plain);
        for (int32 G = 0; G < Gangs; ++G)
        {
            const float Off = (G - (Gangs - 1) * .5f) * 4.6f;
            Gadget(A, U + Off, Z, EHouseGadgetKind::LightSwitch);   // the toggle is the gadget's moving part
        }
    }
    void Outlet(const FHwAnchor& A, float U, float Z)
    {
        ABox(Decor, TEXT("HW_White"), A, U - 3.6f, U + 3.6f, 0, .6f, Z - 5.8f, Z + 5.8f, Plain);
        for (const float Dz : { -2.4f, 2.4f }) ABox(Decor, TEXT("HW_Cavity"), A, U - 1.8f, U + 1.8f, .6f, .9f, Z + Dz - 1.6f, Z + Dz + 1.6f, Plain);
    }
    void FloorRegister(const FHwAnchor& A, float U)
    {
        ABox(Decor, TEXT("HW_White"), A, U - 12.5f, U + 12.5f, 0, 1.2f, 7.f, 20.f, Plain, A.FrontBit());
        AFace(Decor, TEXT("HW_Register"), A, U - 12.5f, U + 12.5f, 1.25f, 7.f, 20.f);
    }
    void FramedPrint(const FHwAnchor& A, float U, float Z, FName Picture, float W, float H, FName FrameKey, float Bar = 2.6f)
    {
        const float D = 2.8f;
        ABox(Decor, FrameKey, A, U - W * .5f, U + W * .5f, 0, D, Z, Z + Bar, Plain);
        ABox(Decor, FrameKey, A, U - W * .5f, U + W * .5f, 0, D, Z + H - Bar, Z + H, Plain);
        ABox(Decor, FrameKey, A, U - W * .5f, U - W * .5f + Bar, 0, D, Z, Z + H, Plain);
        ABox(Decor, FrameKey, A, U + W * .5f - Bar, U + W * .5f, 0, D, Z, Z + H, Plain);
        ABox(Decor, TEXT("HW_White"), A, U - W * .5f + Bar, U + W * .5f - Bar, 0, 1.f, Z + Bar, Z + H - Bar, Plain);
        AFace(Decor, Picture, A, U - W * .5f + Bar, U + W * .5f - Bar, 1.05f, Z + Bar, Z + H - Bar);
    }
    void Poster(const FHwAnchor& A, float U, float Z, FName Picture, float W, float H)
    {
        AFace(Decor, Picture, A, U - W * .5f, U + W * .5f, .3f, Z, Z + H);
        for (const float Cx : { U - W * .5f + 2.f, U + W * .5f - 2.f })
            for (const float Cz : { Z + 2.f, Z + H - 2.f })
                ABox(Decor, TEXT("HW_ClearPlastic"), A, Cx - 2.5f, Cx + 2.5f, .3f, .5f, Cz - 1.2f, Cz + 1.2f, Plain);
    }
    void LevelSign(const FHwAnchor& A, float U, float Z, int32 Level)
    {
        ABox(Decor, TEXT("HW_White"), A, U - 12.f, U + 12.f, 0, .5f, Z, Z + 36.f, Plain, A.FrontBit());
        AFace(Decor, FName(*FString::Printf(TEXT("HW_LevelSign%d"), Level)), A, U - 12.f, U + 12.f, .55f, Z, Z + 36.f);
    }
    /** Black pipe handrail between two wall anchors, Height above the treads, with returns and brackets. */
    void WallRail(const FHwAnchor& A0, const FHwAnchor& A1, float Height = 95.f)
    {
        const float Out = 7.5f, R = 1.9f;
        const FVector P0 = A0.O + A0.N * Out + FVector(0, 0, Height), P1 = A1.O + A1.N * Out + FVector(0, 0, Height);
        ACyl(Decor, TEXT("HW_RailBlack"), A0, P0, P1, R, 12, false);
        ACyl(Decor, TEXT("HW_RailBlack"), A0, P0 - A0.N * (Out + 1.f), P0, R, 12, true);     // returns into the wall
        ACyl(Decor, TEXT("HW_RailBlack"), A1, P1 - A1.N * (Out + 1.f), P1, R, 12, true);
        const float Len = (P1 - P0).Size();
        const int32 Brackets = FMath::Max(2, FMath::RoundToInt(Len / 110.f) + 1);
        for (int32 K = 0; K < Brackets; ++K)
        {
            const FVector C = FMath::Lerp(P0, P1, (K + .5f) / Brackets);
            const float Along = FVector::DotProduct(C - A0.O, A0.U), Z = C.Z - A0.O.Z;
            ABox(Decor, TEXT("HW_RailBlack"), A0, Along - 1.4f, Along + 1.4f, 0.f, Out, Z - 4.6f, Z - 1.2f, Plain);   // arm
            ABox(Decor, TEXT("HW_RailBlack"), A0, Along - 2.6f, Along + 2.6f, 0.f, 1.f, Z - 7.f, Z - 1.f, Plain);      // wall plate
        }
        ++Owner->Placed;
    }
    void SmokeDetector(const FVector& CeilPoint)
    {
        Decor.Cylinder(TEXT("HW_White"), CeilPoint - FVector(0, 0, 3.6f), CeilPoint, 7.f, 18, Plain);
        Decor.BoxMinMax(TEXT("HW_Led"), CeilPoint + FVector(4.5f, -.6f, -4.3f), CeilPoint + FVector(5.7f, .6f, -3.5f), Plain);
    }
    void WallDamage(const FHwAnchor& A, float U, float Z, int32 Kind, float Size, int32 Seed)
    {
        if (!A.bWallPlain) { Owner->Notes += TEXT(" dmg-skip(textured)"); return; }
        UMaterialInterface* M = HwDamageMaterial(Look, A.Wall, A.WallRough, Kind, Seed);
        if (!M) return;
        const FName Key(*FString::Printf(TEXT("HW_Dmg_%d_%d_%d"), Kind, Seed, FMath::RoundToInt(A.Wall.GetLuminance() * 1000)));
        AFace(Decor, Key, A, U - Size * .5f, U + Size * .5f, .12f, Z - Size * .5f, Z + Size * .5f);
    }
    void VacuumDock(const FHwAnchor& A, float U, float Z)
    {
        // Wall charger with the stick vacuum parked in it (purple accents).
        ABox(Decor, TEXT("HW_Black"), A, U - 4.f, U + 4.f, 0, 3.f, Z, Z + 26.f, Plain);
        ABox(Decor, TEXT("HW_PanelGrey"), A, U - 2.4f, U + 2.4f, 3.f, 9.f, Z + 10.f, Z + 12.f, Plain);
        ACyl(Decor, TEXT("HW_PanelGrey"), A, A.P(U, 7.f, Z - 6.f), A.P(U, 7.f, Z + 24.f), 3.6f, 14);
        ACyl(Decor, TEXT("HW_ClearPlastic"), A, A.P(U, 7.f, Z - 1.f), A.P(U, 7.f, Z + 16.f), 4.2f, 14, false);
        ACyl(Decor, TEXT("HW_Purple"), A, A.P(U, 7.f, Z + 24.f), A.P(U, 7.f, Z + 29.f), 3.2f, 14);
        ACyl(Decor, TEXT("HW_Black"), A, A.P(U, 7.f, Z + 29.f), A.P(U, 7.f, Z + 36.f), 1.4f, 10);
        ACyl(Decor, TEXT("HW_Purple"), A, A.P(U, 7.f, Z - 6.f), A.P(U, 7.f, Z - 15.f), 2.6f, 12);
        ACyl(Decor, TEXT("HW_PanelGrey"), A, A.P(U, 7.f, Z - 15.f), A.P(U, 7.f, Z - 40.f), 1.6f, 10);
        ACyl(Decor, TEXT("HW_Purple"), A, A.P(U, 4.f, Z + 27.f), A.P(U, 12.f, Z + 27.f), 1.8f, 10);
    }
};
}

AHouseHallwayDetail::AHouseHallwayDetail()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Root->SetMobility(EComponentMobility::Static);
    SetRootComponent(Root);
}

void AHouseHallwayDetail::Build(UHouseBlackHallSubsystem* Look)
{
    UWorld* World = GetWorld();
    if (!World || !Look) return;
    HwCreateLook(Look);
    FHwBuilder B(World, Look, this);
    const FHwCorridor LongHall{ TEXT("S03_LongHall"), FVector(-2212, -1450, 118), FVector(-2212, 1050, 118), FVector(1, 0, 0), 145.f, 130.f };
    const FHwCorridor ShortHall{ TEXT("S03_ShortHall"), FVector(-1000, -715, 118), FVector(-2300, -715, 118), FVector(0, 1, 0), 155.f, 150.f };
    const FHwCorridor RearHall{ TEXT("S05_RearHall"), FVector(-2195, -1500, 420), FVector(-2195, 1100, 420), FVector(1, 0, 0), 155.f, 125.f };
    const FHwCorridor Landing{ TEXT("S05_Landing"), FVector(-2195, -1500, 420), FVector(-2195, 1100, 420), FVector(1, 0, 0), 0.f, 110.f };
    const FHwCorridor FarHall{ TEXT("S05_FarHall"), FVector(2008, -1500, 420), FVector(2008, 1500, 420), FVector(1, 0, 0), 135.f, 110.f };
    const FHwCorridor FarNorth{ TEXT("S05_FarNorth"), FVector(2008, -1500, 420), FVector(2008, 1500, 420), FVector(1, 0, 0), 245.f, 275.f };
    const FHwCorridor FarSouth{ TEXT("S05_FarSouth"), FVector(2008, -1500, 420), FVector(2008, 1500, 420), FVector(1, 0, 0), 300.f, 220.f };
    const FHwCorridor Gallery{ TEXT("Gallery"), FVector(-1350, -725, 425), FVector(1350, -725, 425), FVector(0, 1, 0), 180.f, 95.f };
    const FHwCorridor UpperShort{ TEXT("UpperShortHall"), FVector(-1150, -725, 420), FVector(-2250, -725, 420), FVector(0, 1, 0), 175.f, 105.f };
    const FHwCorridor NorthLink{ TEXT("UpperNorthLink"), FVector(1150, -725, 420), FVector(2100, -725, 420), FVector(0, 1, 0), 110.f, 0.f };
    FHwAnchor A; FVector Cp; float Fz = 0.f;
    auto At = [&](const FHwCorridor& C, float Side, float T, float Tol = 25.f) { return B.Anchor(C, Side, T, A, Tol); };

    // ================================================================= S03 long hall (ground floor, Y runs along the spine)
    // t = Y + 1450. East wall "+" (X -2067), west wall "-" (X -2342).
    if (At(LongHall, +1, 450)) { B.ExtinguisherCabinet(A, 0, 100.f); }
    if (At(LongHall, +1, 215)) { B.PullStation(A, 0, 118.f); B.HornStrobe(A, 0, 212.f); }
    if (At(LongHall, +1, 160)) { B.LevelSign(A, 0, 165.f, 3); }
    if (At(LongHall, +1, 1950)) { B.ExtinguisherCabinet(A, 0, 100.f); }
    if (At(LongHall, +1, 2160)) { B.PullStation(A, 0, 118.f); B.HornStrobe(A, 0, 212.f); }
    if (At(LongHall, -1, 850)) { B.Thermostat(A, 0, 150.f); B.WallDamage(A, 70.f, 62.f, 0, 60.f, 11); }
    if (At(LongHall, -1, 400)) { B.SwitchPlate(A, 0, 120.f); B.Outlet(A, 60.f, 40.f); }
    if (At(LongHall, -1, 380)) { B.MailPlaque(A, 0, 152.f); }       // beside the second west door (Y -1141)
    if (At(LongHall, -1, 2110)) { B.MailPlaque(A, 0, 152.f); B.SwitchPlate(A, 60.f, 120.f); }
    if (At(LongHall, -1, 1550)) { B.FramedPrint(A, 0, 158.f, TEXT("HW_PicComposite"), 66.f, 52.f, TEXT("HW_FrameDark")); }
    if (At(LongHall, -1, 1000)) { B.Outlet(A, 0, 40.f); B.WallDamage(A, -90.f, 48.f, 2, 70.f, 12); }
    if (At(LongHall, -1, 1800)) { B.Outlet(A, 0, 40.f); }
    if (At(LongHall, +1, 1300)) { B.ConduitVertical(A, 0, 0.f, A.CeilZ - A.O.Z, 198.f); B.ConduitHorizontal(A, 0, 940.f, A.CeilZ - A.O.Z - 6.f); B.FloorRegister(A, -70.f); }
    if (At(LongHall, +1, 1650)) { B.WallDamage(A, 0, 92.f, 3, 64.f, 13); B.Outlet(A, 120.f, 40.f); }
    if (At(LongHall, +1, 300)) { B.Outlet(A, 0, 40.f); }
    if (At(LongHall, +1, 2050)) { B.FramedPrint(A, 0, 165.f, TEXT("HW_PicMountains"), 64.f, 42.f, TEXT("HW_FrameGold")); }
    for (const float T : { 330.f, 1000.f, 1750.f }) if (B.Ceiling(LongHall, T, 0.f, Cp, Fz)) B.SmokeDetector(Cp);
    if (B.Ceiling(LongHall, 140.f, 45.f, Cp, Fz)) B.ExitPendant(Cp, FVector(0, 1, 0), FVector(0, -1, 0), false);
    if (B.Ceiling(LongHall, 2270.f, 45.f, Cp, Fz)) B.ExitPendant(Cp, FVector(0, 1, 0), FVector(0, 1, 0), false, 50.f);
    if (B.Ceiling(LongHall, 760.f, 45.f, Cp, Fz)) B.ExitPendant(Cp, FVector(0, 1, 0), FVector(1, 0, 0), true);

    // ================================================================= S03 short hall (t = -1000 - X); north wall "+" (Y -560), south "-" (Y -865)
    if (At(ShortHall, +1, 480)) { B.ElectricalPanel(A, 0, 118.f); B.WallDamage(A, 50.f, 95.f, 1, 56.f, 14); }
    if (At(ShortHall, +1, 400)) { B.PullStation(A, 0, 118.f); B.HornStrobe(A, 0, 212.f); }
    if (At(ShortHall, +1, 700)) { B.ConduitVertical(A, 0, 0.f, A.CeilZ - A.O.Z, 0.f); B.ConduitHorizontal(A, 0, 340.f, A.CeilZ - A.O.Z - 6.f); B.WallDamage(A, 70.f, 100.f, 0, 46.f, 15); }
    if (At(ShortHall, +1, 950)) { B.Outlet(A, 0, 40.f); B.SwitchPlate(A, 40.f, 120.f); }
    if (At(ShortHall, -1, 900)) { B.Thermostat(A, 0, 150.f); }
    if (At(ShortHall, -1, 560)) { B.SwitchPlate(A, 0, 120.f); B.FloorRegister(A, 60.f); B.WallDamage(A, -60.f, 70.f, 2, 60.f, 16); }
    if (At(ShortHall, -1, 760)) { B.FramedPrint(A, 0, 160.f, TEXT("HW_PicMountains"), 60.f, 40.f, TEXT("HW_FrameDark")); }
    if (At(ShortHall, -1, 420)) { B.Outlet(A, 0, 40.f); }
    if (B.Ceiling(ShortHall, 550.f, 0.f, Cp, Fz)) B.SmokeDetector(Cp);
    if (B.Ceiling(ShortHall, 1000.f, 45.f, Cp, Fz)) B.ExitPendant(Cp, FVector(1, 0, 0), FVector(0, 1, 0), true);

    // ================================================================= S05 rear upper hall (t = Y + 1500); east "+" (X -2040), west "-" (X -2320)
    if (At(RearHall, -1, 950)) { B.LevelSign(A, 0, 140.f, 4); }
    if (At(RearHall, -1, 850)) { B.Poster(A, 0, 150.f, TEXT("HW_PicCorgi"), 60.f, 45.f); }
    if (At(RearHall, -1, 400)) { B.MailPlaque(A, -40.f, 152.f); B.FramedPrint(A, 40.f, 160.f, TEXT("HW_PicDalmatian"), 48.f, 60.f, TEXT("HW_FrameDark")); }
    if (At(RearHall, +1, 370)) { B.PullStation(A, 0, 118.f); B.HornStrobe(A, 0, 212.f); B.MailPlaque(A, 60.f, 152.f); }
    if (At(RearHall, +1, 540)) { B.WallDamage(A, 0, 62.f, 3, 70.f, 21); B.FramedPrint(A, 0, 175.f, TEXT("HW_PicLighthouse"), 92.f, 32.f, TEXT("HW_FrameDark")); B.VacuumDock(A, 75.f, 118.f); }
    if (At(RearHall, +1, 1230)) { B.MailPlaque(A, 0, 152.f); }     // beside the east door (Y -348)
    if (At(RearHall, -1, 1030)) { B.MailPlaque(A, 0, 152.f); B.SwitchPlate(A, -40.f, 120.f); }   // beside room 204 (its door stands open)
    if (At(RearHall, -1, 1350)) { B.Thermostat(A, 0, 150.f); B.WallDamage(A, 70.f, 48.f, 2, 60.f, 22); }
    if (At(RearHall, +1, 1800)) { B.ExtinguisherCabinet(A, 0, 100.f); B.Outlet(A, -60.f, 40.f); }
    if (At(RearHall, +1, 1650)) { B.ConduitVertical(A, 0, 0.f, A.CeilZ - A.O.Z, 190.f); B.ConduitHorizontal(A, 0, 560.f, A.CeilZ - A.O.Z - 6.f); }
    if (At(RearHall, +1, 2250)) { B.PullStation(A, 0, 118.f); B.SwitchPlate(A, 50.f, 120.f); }
    if (At(RearHall, -1, 1930)) { B.MailPlaque(A, 0, 152.f); B.WallDamage(A, -70.f, 40.f, 0, 52.f, 23); }   // beside the bedroom doorway (Y 475..600)
    if (At(RearHall, -1, 1700)) { B.Outlet(A, 0, 40.f); B.FramedPrint(A, 60.f, 165.f, TEXT("HW_PicComposite"), 64.f, 50.f, TEXT("HW_FrameGold")); }
    if (At(RearHall, +1, 1560)) { B.Outlet(A, 0, 40.f); }
    for (const float T : { 700.f, 1400.f, 2100.f }) if (B.Ceiling(RearHall, T, 0.f, Cp, Fz)) B.SmokeDetector(Cp);
    if (B.Ceiling(RearHall, 2280.f, 45.f, Cp, Fz)) B.ExitPendant(Cp, FVector(0, 1, 0), FVector(0, 1, 0), false);
    if (B.Ceiling(RearHall, 820.f, 45.f, Cp, Fz)) B.ExitPendant(Cp, FVector(0, 1, 0), FVector(1, 0, 0), true);
    // Stair landing beyond the door (west wall only).
    if (At(Landing, -1, 2480, 30.f)) { B.FramedPrint(A, 0, 165.f, TEXT("HW_PicLighthouse"), 92.f, 32.f, TEXT("HW_FrameDark")); B.Outlet(A, -80.f, 40.f); }

    // ================================================================= S05 far upper hall (t = Y + 1500); central section east "+" (X 2143), west "-" (X 1898)
    if (At(FarHall, -1, 900)) { B.ExtinguisherCabinet(A, 0, 100.f); }
    if (At(FarHall, +1, 620)) { B.PullStation(A, 0, 118.f); B.HornStrobe(A, 0, 212.f); }
    if (At(FarHall, +1, 1320)) { B.MailPlaque(A, 0, 152.f); }      // beside the east door (opening Y -375..-225)
    if (At(FarHall, -1, 1320)) { B.MailPlaque(A, 0, 152.f); B.SwitchPlate(A, 50.f, 120.f); }
    if (At(FarHall, -1, 1600)) { B.Thermostat(A, 0, 150.f); B.WallDamage(A, -80.f, 50.f, 2, 60.f, 31); }
    if (At(FarHall, +1, 1500)) { B.FramedPrint(A, 0, 160.f, TEXT("HW_PicComposite"), 66.f, 52.f, TEXT("HW_FrameDark")); B.Outlet(A, 90.f, 40.f); }
    if (At(FarHall, +1, 1750)) { B.WallDamage(A, 0, 70.f, 0, 56.f, 32); }
    if (At(FarHall, -1, 1000)) { B.Outlet(A, 40.f, 40.f); B.Poster(A, -60.f, 150.f, TEXT("HW_PicCorgi"), 60.f, 45.f); }
    if (At(FarHall, -1, 1850)) { B.PullStation(A, 0, 118.f); }
    if (At(FarHall, +1, 1000)) { B.ConduitVertical(A, 0, 0.f, A.CeilZ - A.O.Z, 196.f); B.ConduitHorizontal(A, 0, 850.f, A.CeilZ - A.O.Z - 6.f); }
    if (At(FarNorth, +1, 2550, 40.f)) { B.ExtinguisherCabinet(A, 0, 100.f); }
    if (At(FarNorth, -1, 2500, 40.f)) { B.PullStation(A, 0, 118.f); B.HornStrobe(A, 0, 212.f); B.FramedPrint(A, 90.f, 160.f, TEXT("HW_PicMountains"), 64.f, 42.f, TEXT("HW_FrameGold")); }
    if (At(FarSouth, -1, 150, 40.f)) { B.LevelSign(A, 0, 140.f, 4); B.Outlet(A, 70.f, 40.f); }
    if (At(FarSouth, +1, 300, 40.f)) { B.WallDamage(A, 0, 60.f, 1, 56.f, 33); }
    for (const float T : { 750.f, 1450.f, 2450.f }) if (B.Ceiling(FarHall, T, 0.f, Cp, Fz)) B.SmokeDetector(Cp);
    if (B.Ceiling(FarHall, 600.f, 45.f, Cp, Fz)) B.ExitPendant(Cp, FVector(0, 1, 0), FVector(0, -1, 0), false);
    if (B.Ceiling(FarHall, 1750.f, 45.f, Cp, Fz)) B.ExitPendant(Cp, FVector(0, 1, 0), FVector(0, 1, 0), false);

    // ================================================================= Gallery (t = X + 1350); north wall "+" (Y -545) only at the ends, south "-" (Y -820)
    if (At(Gallery, +1, 180)) { B.PullStation(A, 0, 118.f); B.HornStrobe(A, 0, 212.f); }
    if (At(Gallery, +1, 500)) { B.ExtinguisherCabinet(A, 0, 100.f); B.Outlet(A, -70.f, 40.f); }
    if (At(Gallery, +1, 340)) { B.FramedPrint(A, 0, 160.f, TEXT("HW_PicMountains"), 64.f, 42.f, TEXT("HW_FrameDark")); }
    if (At(Gallery, +1, 2200)) { B.Thermostat(A, 0, 150.f); B.WallDamage(A, 70.f, 50.f, 2, 60.f, 41); }
    if (At(Gallery, +1, 2450)) { B.Outlet(A, 0, 40.f); B.PullStation(A, 60.f, 118.f); }
    if (At(Gallery, -1, 650)) { B.SwitchPlate(A, 0, 120.f); }
    for (const float T : { 500.f, 1350.f, 2200.f }) if (B.Ceiling(Gallery, T, 0.f, Cp, Fz)) B.SmokeDetector(Cp);
    if (B.Ceiling(Gallery, 120.f, 45.f, Cp, Fz)) B.ExitPendant(Cp, FVector(1, 0, 0), FVector(-1, 0, 0), false);

    // ================================================================= Upper short hall (t = -1150 - X); north "+" (Y -550), south "-" (Y -825)
    if (At(UpperShort, +1, 750)) { B.PullStation(A, 0, 118.f); B.HornStrobe(A, 0, 212.f); }
    if (At(UpperShort, +1, 450)) { B.FramedPrint(A, 0, 160.f, TEXT("HW_PicComposite"), 66.f, 52.f, TEXT("HW_FrameGold")); B.SwitchPlate(A, -70.f, 120.f); }
    if (At(UpperShort, -1, 400)) { B.MailPlaque(A, 0, 152.f); B.Outlet(A, 60.f, 40.f); }
    if (At(UpperShort, -1, 600)) { B.WallDamage(A, 0, 72.f, 1, 60.f, 42); }
    if (B.Ceiling(UpperShort, 550.f, 0.f, Cp, Fz)) B.SmokeDetector(Cp);

    // ================================================================= Upper north link (t = X - 1150); north wall "+" (Y -610)
    if (At(NorthLink, +1, 620)) { B.PullStation(A, 0, 118.f); B.HornStrobe(A, 0, 212.f); }
    if (At(NorthLink, +1, 420)) { B.FramedPrint(A, 0, 160.f, TEXT("HW_PicLighthouse"), 92.f, 32.f, TEXT("HW_FrameDark")); B.Outlet(A, -90.f, 40.f); }
    if (At(NorthLink, +1, 300)) { B.WallDamage(A, 0, 55.f, 2, 60.f, 43); }
    if (B.Ceiling(NorthLink, 230.f, 45.f, Cp, Fz)) B.ExitPendant(Cp, FVector(1, 0, 0), FVector(1, 0, 0), false);

    // The stairwell masonry is continuous: wall-side rails are removed here.
    // Open-side flight and landing guards remain in the house geometry.
    // Stairwell ceilings: a detector over each landing.
    const FHwCorridor StairLandings{ TEXT("S03_StairLandings"), FVector(-2150, 1050, 425), FVector(-1500, 1050, 289), FVector(0, 1, 0), 0.f, 0.f };
    if (B.Ceiling(StairLandings, 0.f, 0.f, Cp, Fz)) B.SmokeDetector(Cp);
    if (B.Ceiling(StairLandings, (StairLandings.B - StairLandings.A).Size(), 0.f, Cp, Fz)) B.SmokeDetector(Cp);
    const FHwCorridor BasementLanding{ TEXT("S03_BasementLanding"), FVector(-1500, 1050, -45), FVector(-1500, 1050, -45), FVector(0, 1, 0), 0.f, 0.f };
    if (B.Ceiling(BasementLanding, 0.f, 0.f, Cp, Fz)) B.SmokeDetector(Cp);

    // ---------------------------------------------------------------- the singing bass: bare wall right beside the Bud Light flag
    {
        AHouseInteractiveProp* Flag = nullptr;
        for (TActorIterator<AHouseInteractiveProp> It(World); It; ++It)
            if (It->IntactMesh && It->IntactMesh->GetName().Contains(TEXT("BudLightFlag"))) { Flag = *It; break; }
        bool bBass = false;
        if (Flag && Flag->Body)
        {
            const FBox WorldFlagBounds = Flag->Body->Bounds.GetBox();
            const FBox FB(HouseWorldScale::Unpoint(World, WorldFlagBounds.Min),
                HouseWorldScale::Unpoint(World, WorldFlagBounds.Max));
            const FVector FC = FB.GetCenter();
            const FVector Dir = (RearHall.B - RearHall.A).GetSafeNormal();
            const float Tf = FVector::DotProduct(FC - RearHall.A, Dir);
            const float Side = FVector::DotProduct(FC - RearHall.A, RearHall.Side) > 0.f ? 1.f : -1.f;
            const float HalfAlong = FVector::DotProduct(FB.GetExtent(), Dir.GetAbs());
            // Try the near side of the flag first (toward the recliner nook), then the far side, stepping out until the wall is bare.
            const float Nook = 545.f;
            const float First = Tf > Nook ? -1.f : 1.f;
            for (int32 Step = 0; Step < 4 && !bBass; ++Step)
                for (float Dirn : { First, -First })
                {
                    const float Tc = Tf + Dirn * (HalfAlong + 8.f + 19.f + Step * 12.f);
                    FHwAnchor BA;
                    if (!B.Anchor(RearHall, Side, Tc, BA, 30.f)) continue;
                    const float Z = FMath::Clamp(FC.Z - BA.O.Z, 135.f, 160.f);
                    if (!B.BassClear(BA, 0.f, Z)) continue;
                    B.BillyBass(BA, 0.f, Z);
                    Notes += FString::Printf(TEXT(" bass@t=%.0f(flag t=%.0f half=%.0f side=%.0f)"), Tc, Tf, HalfAlong, Side);
                    bBass = true;
                    break;
                }
        }
        if (!bBass) Notes += Flag ? TEXT(" bass:no-bare-wall") : TEXT(" bass:no-flag");
    }

    // ---------------------------------------------------------------- components
    auto Make = [&](const FHallMesh& M, const TCHAR* Name, bool bCollision)
    {
        if (M.NumTriangles() == 0) return;
        UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this, Name);
        PM->SetupAttachment(RootComponent);
        PM->SetMobility(EComponentMobility::Static);
        PM->bUseComplexAsSimpleCollision = true;
        PM->bUseAsyncCooking = false;
        if (bCollision) { PM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName); PM->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); }
        else PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PM->RegisterComponent();
        M.Build(PM, [Look](FName Key) { return Look->Mat(Key); }, bCollision);
        Meshes.Add(PM);
        Triangles += M.NumTriangles();
    };
    Make(B.Decor, TEXT("HallwayDecor"), false);
    Make(B.Bulky, TEXT("HallwayBulky"), true);
    UE_LOG(LogTemp, Display, TEXT("HOUSE_HALLWAY_DETAIL placed=%d skipped=%d tris=%d gadgets=%d%s"), Placed, Skipped, Triangles, GadgetSpecs.Num(), *Notes);
}
