#include "HouseWoodsSilhouettes.h"
#include "HouseCalderMapTrees.h"
#include "HouseWoodsDetail.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseDetail.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace HouseWoodsSilhouettesImpl
{
const FName Bark(TEXT("EX_WoodsPassBark0"));
const FName Bare(TEXT("EX_WoodsSilhouetteBare"));
const FName Hollow(TEXT("EX_WoodsSilhouetteHollow"));
const FHallUV UV = FHallUV::World(130.f);

void CreateLook(UHouseBlackHallSubsystem* Look)
{
    UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropPlain.M_NaturePropPlain"));
    auto Plain = [&](FName Key, FLinearColor Tint)
    {
        if (Look->Materials.Contains(Key)) return;
        if (!Parent) { HallMakePlain(Look, Key, Tint, .99f); return; }
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, Look);
        Material->SetVectorParameterValue(TEXT("Tint"), Tint);
        Material->SetVectorParameterValue(TEXT("Emission"), FLinearColor::Black);
        Material->SetScalarParameterValue(TEXT("Roughness"), .99f);
        Material->SetScalarParameterValue(TEXT("Metallic"), 0);
        Look->Materials.Add(Key, Material);
    };
    Plain(Bare, FLinearColor(.17f, .135f, .090f));
    Plain(Hollow, FLinearColor(.023f, .016f, .009f));
}

void Taper(FHallMesh& Mesh, FName Key, const TArray<FVector>& Points, const TArray<float>& Radii, int Sides)
{
    if (Points.Num() < 2 || Points.Num() != Radii.Num()) return;
    FHallSection& Section = Mesh.Sec(Key);
    const int StartVertex = Section.V.Num();
    float Along = 0;
    for (int Ring = 0; Ring < Points.Num(); ++Ring)
    {
        const FVector Axis = (Points[FMath::Min(Points.Num() - 1, Ring + 1)] - Points[FMath::Max(0, Ring - 1)]).GetSafeNormal();
        const FVector Across = FVector::CrossProduct(Axis, FMath::Abs(Axis.Z) < .9f ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
        const FVector Around = FVector::CrossProduct(Axis, Across).GetSafeNormal();
        if (Ring) Along += (Points[Ring] - Points[Ring - 1]).Size() / 130.f;
        for (int Side = 0; Side <= Sides; ++Side)
        {
            const float Angle = Side * 2 * PI / Sides;
            const FVector Normal = Across * FMath::Cos(Angle) + Around * FMath::Sin(Angle);
            Section.V.Add(Points[Ring] + Normal * Radii[Ring] * (1 + .06f * FMath::Sin(Side * 2.8f + Ring)));
            Section.N.Add(Normal);
            Section.UV.Add({float(Side) / Sides * 2, -Along});
            Section.T.Add(FProcMeshTangent((Around * FMath::Cos(Angle) - Across * FMath::Sin(Angle)).GetSafeNormal(), false));
        }
    }
    for (int Ring = 0; Ring + 1 < Points.Num(); ++Ring) for (int Side = 0; Side < Sides; ++Side)
    {
        const int A = StartVertex + Ring * (Sides + 1) + Side, B = A + 1, C = B + Sides + 1, D = A + Sides + 1;
        if (FVector::DotProduct(FVector::CrossProduct(Section.V[B] - Section.V[A], Section.V[C] - Section.V[A]), Section.N[A]) < 0)
            Section.I.Append({A, B, C, A, C, D});
        else Section.I.Append({A, C, B, A, D, C});
    }
}

void Snag(FHallMesh& Mesh, int Variant, TArray<FBox>& Collision)
{
    const float Mirror = Variant % 2 ? -1.f : 1.f;
    const TArray<FVector> Spine = {{0,0,-8},{6,-15,250},{-27,4,510},{23,18,735},{-11,29,945},{-86,44,1180},{-63,57,1310}};
    TArray<FVector> Stem;
    for (FVector Point : Spine) { Point.X *= Mirror; Stem.Add(Point); }
    Taper(Mesh, Bark, Stem, {37,31,24,19,13,7,1.2f}, 10);
    Taper(Mesh, Bark, {Stem[2], FVector(113 * Mirror, 35, 666), FVector(196 * Mirror, 69, 863), FVector(245 * Mirror, 58, 973)}, {18,11,5,.55f}, 7);
    Taper(Mesh, Bare, {FVector(196 * Mirror,69,863), FVector(285 * Mirror,37,929), FVector(296 * Mirror,-8,962)}, {4.8f,2.1f,.3f}, 5);
    Taper(Mesh, Bark, {Stem[3], FVector(-145 * Mirror,-19,817), FVector(-207 * Mirror,-40,980), FVector(-251 * Mirror,-26,1038)}, {12,8,3,.4f}, 7);
    Taper(Mesh, Bark, {Stem[4], FVector(74 * Mirror,-54,1025), FVector(128 * Mirror,-91,1202)}, {8,3,.35f}, 6);
    Taper(Mesh, Bare, {Stem[5], FVector(-184 * Mirror,104,1256), FVector(-203 * Mirror,148,1263)}, {5,1.8f,.2f}, 5);
    Taper(Mesh, Bark, {Stem[1], FVector(87 * Mirror,-69,334), FVector(117 * Mirror,-72,341)}, {12,6,.8f}, 7);
    // The short sawn-looking ends are torn into individual pale splinters.
    for (int Splinter = 0; Splinter < 5; ++Splinter)
    {
        const float Angle = Splinter * 2.39996f;
        const FVector Base = Stem[5] + FVector(FMath::Cos(Angle) * 5, FMath::Sin(Angle) * 5, 0);
        Taper(Mesh, Bare, {Base, Base + FVector(FMath::Cos(Angle) * 9, FMath::Sin(Angle) * 8, 21 + Splinter * 7)}, {1.8f,.12f}, 4);
    }
    for (int Root = 0; Root < 5; ++Root)
    {
        const float Angle = Root * 2.39996f;
        const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0);
        Taper(Mesh, Bark, {{0,0,57}, Out * 46 + FVector(0,0,10), Out * 91 + FVector(0,0,-8)}, {12,6,.8f}, 6);
    }
    // Only the lowest stem blocks a wandering player; high limbs never create
    // broad invisible walls. The whole group is outside all protected routes.
    Collision.Add(FBox(FVector(-29,-36,-5), FVector(32,26,190)));
}

void BrokenStump(FHallMesh& Mesh, TArray<FBox>& Collision)
{
    Taper(Mesh, Bark, {{0,0,-5},{-4,2,39},{7,-2,101},{2,5,151}}, {40,32,25,19}, 12);
    // Hollow black centre and an uneven pale rim, never facial features.
    Mesh.Cylinder(Hollow, {2,5,105}, {2,5,110}, 18, 12, UV);
    for (int Side = 0; Side < 12; ++Side)
    {
        const float Angle = Side * 2 * PI / 12, Next = (Side + 1) * 2 * PI / 12;
        const float Height = 148 + 17 * FMath::Sin(Side * 2.71f);
        const float NextHeight = 148 + 17 * FMath::Sin((Side + 1) * 2.71f);
        const FVector A(FMath::Cos(Angle) * 21 + 2, FMath::Sin(Angle) * 21 + 5, Height);
        const FVector B(FMath::Cos(Next) * 21 + 2, FMath::Sin(Next) * 21 + 5, NextHeight);
        const FVector C(FMath::Cos(Next) * 15 + 2, FMath::Sin(Next) * 15 + 5, NextHeight - 4);
        const FVector D(FMath::Cos(Angle) * 15 + 2, FMath::Sin(Angle) * 15 + 5, Height - 4);
        Mesh.Quad(Bare, A, B, C, D, FVector::UpVector, UV);
        Mesh.Quad(Hollow, D, C, C - FVector(0,0,43), D - FVector(0,0,43), FVector(-FMath::Cos(Angle),-FMath::Sin(Angle),0), UV);
    }
    // A naturally projecting broken limb can momentarily read as a shoulder.
    Taper(Mesh, Bark, {{0,0,111},{29,-4,123},{49,-2,116}}, {13,9,3}, 7);
    Taper(Mesh, Bark, {{-4,0,68},{-32,9,75},{-43,16,69}}, {10,7,2}, 7);
    Collision.Add(FBox(FVector(-32,-27,-5), FVector(31,27,139)));
}

void LeaningPair(FHallMesh& Mesh, const FVector& Difference, TArray<FBox>& Collision)
{
    const FVector Contact = Difference + FVector(-28, -10, 706);
    const FVector Mid = Contact * .47f + FVector(-20, 26, -19);
    // The snapped tip bends around the near side of its support, never straight
    // through the standing trunk after their point of contact.
    Taper(Mesh, Bark, {{0,0,-6}, Mid, Contact, Contact + FVector(-70, 50, 80)}, {31,22,12,2.5f}, 11);
    const FVector Axis = Contact.GetSafeNormal(), Side = FVector::CrossProduct(Axis, FVector::UpVector).GetSafeNormal();
    for (int Branch = 0; Branch < 4; ++Branch)
    {
        const FVector Start = FMath::Lerp(Mid, Contact, .12f + Branch * .19f);
        const float Sign = Branch % 2 ? 1.f : -1.f;
        const FVector End = Start + Side * Sign * (98 + Branch * 20) + FVector(0,0,41 + Branch * 8);
        Taper(Mesh, Bark, {Start, FMath::Lerp(Start, End, .62f) + FVector(0,0,21), End}, {8.f - Branch,3.f,.35f}, 6);
    }
    Mesh.Cylinder(Bare, {0,0,0}, {-2,-2,3}, 29, 12, UV);
    // An original, separate crooked standing tree visibly supports the fallen
    // trunk. Its roots are checked independently; no existing tree is moved.
    auto At = [&](FVector Local) { return Difference + Local; };
    Taper(Mesh, Bark, {At({0,0,-6}), At({7,-13,253}), At({-12,-8,568}), At({10,9,828}), At({67,14,1110}), At({111,41,1288})},
        {38,32,25,18,10,1.3f}, 10);
    Taper(Mesh, Bark, {At({10,9,828}), At({-71,25,933}), At({-155,59,1195})}, {16,10,1.4f}, 7);
    Taper(Mesh, Bark, {At({-12,-8,568}), At({82,4,642}), At({147,36,833})}, {14,7,1}, 7);
    Taper(Mesh, Bare, {At({147,36,833}), At({223,57,923})}, {4,.3f}, 5);
    Taper(Mesh, Bark, {At({67,14,1110}), At({117,-74,1202}), At({164,-106,1224})}, {7,3,.4f}, 6);
    Collision.Add(FBox(FVector(-25,-25,-5), FVector(27,29,75)));
    Collision.Add(FBox(Difference + FVector(-30,-34,-4), Difference + FVector(33,25,190)));
}

TArray<FVector> TreeRoots(AActor* Owner)
{
    TArray<FVector> Result;
    TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components(Owner);
    for (auto* Component : Components)
    {
        const FString Name = Component->GetName();
        const bool Original = Name.StartsWith(TEXT("ExteriorTree")) || Name.StartsWith(TEXT("ExteriorForestDistant"));
        const bool Added = Name.StartsWith(TEXT("ExteriorWoodsPass")) && !Name.Contains(TEXT("Shrub")) && !Name.Contains(TEXT("Sapling"));
        if (!Original && !Added) continue;
        for (int Instance = 0; Instance < Component->GetInstanceCount(); ++Instance)
        {
            FTransform Transform;
            Component->GetInstanceTransform(Instance, Transform, true);
            Result.Add(Transform.GetLocation());
        }
    }
    return Result;
}

bool Safe(FVector2D Point, float Radius, const TArray<FVector>& Occupied)
{
    if (Point.Y < 6800 || Point.Y > 18350 || FMath::Abs(Point.X) + Radius > 7700) return false;
    if (HouseWoods::TrailDistance(Point.X, Point.Y) < 450 + Radius) return false;
    if (FVector2D((Point.X - HouseWoods::HutX) / 1.25f, Point.Y - HouseWoods::HutY).Size() < 1250 + Radius) return false;
    for (const HouseWoods::FHollow& Depression : HouseWoods::Hollows())
        if ((Point - Depression.Center).Size() < Depression.Radius + Radius) return false;
    for (const FVector& Trunk : Occupied)
        if ((Point - FVector2D(Trunk.X, Trunk.Y)).Size() < 210) return false;
    for (float X = -7000; X < 8000; X += 1800) for (float Y = 5000; Y < 18500; Y += 2100)
        if ((Point - FVector2D(X,Y)).Size() < 115) return false;
    return true;
}

bool FindPosition(FVector2D Desired, float Radius, const TArray<FVector>& Occupied, FVector& Found,
    const FVector2D* SecondOffset = nullptr)
{
    for (int Candidate = 0; Candidate < 33; ++Candidate)
    {
        const float Angle = Candidate * 2.39996f;
        const float Distance = Candidate == 0 ? 0 : 150 + ((Candidate - 1) / 8) * 110;
        const FVector2D Point = Desired + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;
        if (!Safe(Point, Radius, Occupied)) continue;
        if (SecondOffset && !Safe(Point + *SecondOffset, 265, Occupied)) continue;
        Found = FVector(Point.X, Point.Y, HouseWoods::Height(Point.X, Point.Y));
        return true;
    }
    return false;
}

void Batch(AActor* Owner, UHouseBlackHallSubsystem* Look, const TCHAR* Name, FHallMesh& Mesh,
    const TArray<FBox>& Collision, const TArray<FTransform>& Instances, int Cull)
{
    if (Instances.IsEmpty()) return;
    UStaticMesh* Built = HwBuildStaticMesh(Mesh, *FString::Printf(TEXT("SM_%s"), Name), Look, Collision);
    if (!Built) return;
    Built=HouseCalderMapTrees::Mesh(Owner,Look,Name,Built,Instances);
    auto* Component = NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner, FName(Name), RF_Transient);
    Owner->AddInstanceComponent(Component);
    Component->SetupAttachment(Owner->GetRootComponent());
    Component->SetMobility(EComponentMobility::Static);
    Component->SetStaticMesh(Built);
    Component->SetCastShadow(false);
    Component->bAffectDynamicIndirectLighting = false;
    Component->bAffectDistanceFieldLighting = false;
    Component->SetGenerateOverlapEvents(false);
    Component->SetCullDistances(int(Cull * .84f), Cull);
    Component->RegisterComponent();
    Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    Component->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Component->AddInstances(Instances, false, true);
    Component->BuildTreeIfOutdated(false, true);
    UE_LOG(LogTemp, Display, TEXT("WOODS_SILHOUETTE_BATCH name=%s count=%d templateTriangles=%d cull=%d simpleTrunkBoxes=%d lights=0"),
        Name, Instances.Num(), Mesh.NumTriangles(), Cull, Collision.Num());
}
}

void HouseWoodsSilhouettes::Build(AActor* Owner, UHouseBlackHallSubsystem* Look)
{
    if (!Owner || !Look || !Look->Materials.Contains(HouseWoodsSilhouettesImpl::Bark)) return;
    HouseWoodsSilhouettesImpl::CreateLook(Look);
    TArray<FVector> Occupied = HouseWoodsSilhouettesImpl::TreeRoots(Owner);
    TArray<FTransform> SnagTransforms[2];
    const FVector2D DesiredSnags[] = {{-4130,10730},{450,10200},{-5550,15150},{3800,17400}};
    for (int SnagIndex = 0; SnagIndex < 4; ++SnagIndex)
    {
        FVector Position;
        const float Scale = SnagIndex == 0 ? 1.03f : SnagIndex == 1 ? .81f : SnagIndex == 2 ? 1.1f : 1.2f;
        if (!HouseWoodsSilhouettesImpl::FindPosition(DesiredSnags[SnagIndex], 365 * Scale, Occupied, Position))
        { UE_LOG(LogTemp, Warning, TEXT("WOODS_SILHOUETTE no safe snag position index=%d"), SnagIndex); continue; }
        Occupied.Add(Position);
        SnagTransforms[SnagIndex % 2].Add(FTransform(FRotator(0, SnagIndex == 0 ? 37.f : SnagIndex * 113.f, 0), Position, FVector(Scale)));
        UE_LOG(LogTemp, Display, TEXT("WOODS_SILHOUETTE snag=%d pos=%s trailClearance=%.1f"), SnagIndex, *Position.ToCompactString(), HouseWoods::TrailDistance(Position.X, Position.Y));
    }
    for (int Variant = 0; Variant < 2; ++Variant)
    {
        FHallMesh Mesh;
        TArray<FBox> Collision;
        HouseWoodsSilhouettesImpl::Snag(Mesh, Variant, Collision);
        HouseWoodsSilhouettesImpl::Batch(Owner, Look, *FString::Printf(TEXT("ExteriorAppalachianBareSnags%d"), Variant), Mesh, Collision, SnagTransforms[Variant], 19500);
    }
    const FVector2D PairOffset(370, 600);
    FVector FallenFoot;
    if (HouseWoodsSilhouettesImpl::FindPosition({-3280,12100}, 250, Occupied, FallenFoot, &PairOffset))
    {
        const FVector StandingFoot(FallenFoot.X + PairOffset.X, FallenFoot.Y + PairOffset.Y,
            HouseWoods::Height(FallenFoot.X + PairOffset.X, FallenFoot.Y + PairOffset.Y));
        FHallMesh Mesh;
        TArray<FBox> Collision;
        HouseWoodsSilhouettesImpl::LeaningPair(Mesh, StandingFoot - FallenFoot, Collision);
        HouseWoodsSilhouettesImpl::Batch(Owner, Look, TEXT("ExteriorAppalachianLeaningDeadwood"), Mesh, Collision, {FTransform(FallenFoot)}, 12500);
        Occupied.Add(FallenFoot);
        Occupied.Add(StandingFoot);
        UE_LOG(LogTemp, Display, TEXT("WOODS_SILHOUETTE leaningFoot=%s support=%s contactHeight=706"), *FallenFoot.ToCompactString(), *StandingFoot.ToCompactString());
    }
    else UE_LOG(LogTemp, Warning, TEXT("WOODS_SILHOUETTE no safe leaning pair position"));
    FVector StumpPosition;
    if (HouseWoodsSilhouettesImpl::FindPosition({-2450,12170}, 85, Occupied, StumpPosition))
    {
        FHallMesh Mesh;
        TArray<FBox> Collision;
        HouseWoodsSilhouettesImpl::BrokenStump(Mesh, Collision);
        HouseWoodsSilhouettesImpl::Batch(Owner, Look, TEXT("ExteriorAppalachianAmbiguousStump"), Mesh, Collision,
            {FTransform(FRotator(0,128,0), StumpPosition)}, 6800);
        UE_LOG(LogTemp, Display, TEXT("WOODS_SILHOUETTE hollowStump=%s height=165 noHumanFeatures=1"), *StumpPosition.ToCompactString());
    }
}
