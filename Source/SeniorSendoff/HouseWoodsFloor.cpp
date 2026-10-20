#include "HouseWoodsFloor.h"
#include "HouseCalderLandscape.h"

#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseDetail.h"
#include "HouseStreetScene.h"
#include "HouseWoodsDetail.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace HouseWoodsFloorImpl
{
constexpr int32 TextureSize = 1024;
constexpr int32 FloorSeed = 202610131;
const FName BarkKey(TEXT("EX_WoodsFloorBark"));
const FName MossKey(TEXT("EX_WoodsFloorMoss"));
const FName FernKey(TEXT("EX_WoodsFloorFern"));
const FName StoneKey(TEXT("EX_WoodsFloorStone"));
const FName SoilKey(TEXT("EX_WoodsFloorSoil"));
const FName EndKey(TEXT("EX_WoodsFloorRot"));
const FHallUV DetailUV = FHallUV::World(65.f);

UMaterialInstanceDynamic* Plain(UHouseBlackHallSubsystem* Look, FName Key, FLinearColor Tint)
{
    UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropPlain.M_NaturePropPlain"));
    if (!Parent) return HallMakePlain(Look, Key, Tint, .97f);
    auto* Material = UMaterialInstanceDynamic::Create(Parent, Look);
    Material->SetVectorParameterValue(TEXT("Tint"), Tint);
    Material->SetVectorParameterValue(TEXT("Emission"), FLinearColor::Black);
    Material->SetScalarParameterValue(TEXT("Roughness"), .97f);
    Material->SetScalarParameterValue(TEXT("Metallic"), 0.f);
    Look->Materials.Add(Key, Material);
    return Material;
}

UMaterialInstanceDynamic* Textured(UHouseBlackHallSubsystem* Look, FName Key, UTexture2D* Base, UTexture2D* Normal)
{
    UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured.M_NaturePropTextured"));
    if (!Parent) return HallMakeTextured(Look, Key, Base, Normal, .98f, FLinearColor::White, 1.1f);
    auto* Material = UMaterialInstanceDynamic::Create(Parent, Look);
    Material->SetTextureParameterValue(TEXT("BaseMap"), Base);
    Material->SetTextureParameterValue(TEXT("NormalMap"), Normal);
    Material->SetVectorParameterValue(TEXT("Tint"), FLinearColor::White);
    Material->SetVectorParameterValue(TEXT("Emission"), FLinearColor::Black);
    Material->SetScalarParameterValue(TEXT("UVScale"), 1.f);
    Material->SetScalarParameterValue(TEXT("Roughness"), .98f);
    Material->SetScalarParameterValue(TEXT("NormalStrength"), 1.1f);
    Look->Materials.Add(Key, Material);
    return Material;
}

// Four original leaf masks, measured along their midrib. Leaves are drawn into
// a wrapping tile in depth order; their edges, veins and curl also paint relief.
float LeafWidth(float Along, int32 Species)
{
    if (Along < 0.f || Along > 1.f) return 0.f;
    const float Envelope = FMath::Pow(FMath::Max(0.f, FMath::Sin(Along * PI)), .72f);
    if (Species == 0) return Envelope * (.30f + .13f * FMath::Cos(Along * PI * 9.f)); // lobed oak
    if (Species == 1) return Envelope * .245f * (.97f + .03f * FMath::Cos(Along * PI * 35.f)); // hickory leaflet
    if (Species == 2) return Envelope * (.27f + .35f * FMath::Pow(FMath::Abs(FMath::Sin(Along * PI * 3.f)), 4.f)); // maple lobes
    return Envelope * (Along > .76f ? .27f : .48f) * (1.f - .30f * FMath::Exp(-FMath::Square((Along - .49f) / .10f))); // tulip poplar
}

void PaintFloor(FHallImage& Image, FHallImage& Normal)
{
    Image.Init(TextureSize, TextureSize, FLinearColor::Black);
    TArray<float> Relief;
    Relief.SetNum(TextureSize * TextureSize);
    for (int32 PixelY = 0; PixelY < TextureSize; ++PixelY)
        for (int32 PixelX = 0; PixelX < TextureSize; ++PixelX)
        {
            const float Grain = HallNoise(PixelX * .93f, PixelY * .93f, FloorSeed);
            const float Soil = .036f + .027f * HallFbm(PixelX * .038f, PixelY * .038f, 3, FloorSeed + 1);
            Image.At(PixelX, PixelY) = FLinearColor(Soil * 1.18f, Soil * .78f, Soil * .47f) * (.82f + Grain * .38f);
            Relief[PixelY * TextureSize + PixelX] = .035f * Grain;
        }
    const FLinearColor Palette[] = {
        FLinearColor(.235f, .124f, .039f), FLinearColor(.17f, .084f, .026f),
        FLinearColor(.31f, .188f, .066f), FLinearColor(.115f, .092f, .033f),
        FLinearColor(.18f, .053f, .026f), FLinearColor(.105f, .049f, .021f)};
    FRandomStream PaintRng(FloorSeed + 2);
    for (int32 LeafIndex = 0; LeafIndex < 1900; ++LeafIndex)
    {
        const float CenterX = PaintRng.FRandRange(0, TextureSize), CenterY = PaintRng.FRandRange(0, TextureSize);
        const float Length = PaintRng.FRandRange(31, 81), Angle = PaintRng.FRandRange(0, 2 * PI);
        const float CosAngle = FMath::Cos(Angle), SinAngle = FMath::Sin(Angle);
        const int32 Species = LeafIndex % 4, Extent = FMath::CeilToInt(Length * .8f);
        const FLinearColor LeafColor = Palette[PaintRng.RandRange(0, UE_ARRAY_COUNT(Palette) - 1)] * PaintRng.FRandRange(.68f, 1.18f);
        for (int32 OffsetY = -Extent; OffsetY <= Extent; ++OffsetY)
            for (int32 OffsetX = -Extent; OffsetX <= Extent; ++OffsetX)
            {
                const float Along = (OffsetX * CosAngle + OffsetY * SinAngle) / Length + .5f;
                const float Across = (-OffsetX * SinAngle + OffsetY * CosAngle) / Length;
                const float Width = LeafWidth(Along, Species);
                if (Width <= 0.f || FMath::Abs(Across) > Width) continue;
                const int32 PixelX = (FMath::FloorToInt(CenterX) + OffsetX + TextureSize) % TextureSize;
                const int32 PixelY = (FMath::FloorToInt(CenterY) + OffsetY + TextureSize) % TextureSize;
                const float Edge = FMath::Clamp((Width - FMath::Abs(Across)) * Length / 2.f, 0.f, 1.f);
                const float VeinDistance = FMath::Abs(FMath::Frac(Along * 8.f - FMath::Abs(Across) * 2.1f) - .5f);
                const float Veins = (FMath::Abs(Across) < .014f ? .11f : 0.f) + (VeinDistance < .035f ? .038f : 0.f);
                const float Speckle = HallNoise(PixelX * .77f, PixelY * .77f, FloorSeed + 7);
                const float Curl = .80f + .18f * FMath::Abs(Across) / FMath::Max(.01f, Width);
                Image.At(PixelX, PixelY) = FMath::Lerp(Image.At(PixelX, PixelY), LeafColor * (Curl + Veins + Speckle * .17f) * (.57f + .43f * Edge), Edge);
                Relief[PixelY * TextureSize + PixelX] = .10f + .13f * Edge + .055f * FMath::Abs(Across) + Veins * .50f + Speckle * .018f;
            }
    }
    HallHeightToNormal(Relief, TextureSize, TextureSize, 2.0f, true, Normal);
}

FVector OnGround(float X, float Y, float Lift = 0.f)
{
    return FVector(X, Y, HouseWoods::Height(X, Y) + Lift);
}

bool Allowed(float X, float Y, float Margin = 375.f)
{
    return Y > 4900.f && Y < 18800.f && FMath::Abs(X) < 7900.f
        // Two-metre allowance keeps fern/root/boulder extents out of the
        // west-gate corridor, not merely their instance origins.
        && !(X < -6000.f && Y < 7000.f)
        && HouseWoods::TrailDistance(X, Y) > Margin
        && FVector2D((X - HouseWoods::HutX) / 1.25f, Y - HouseWoods::HutY).Size() > 1050.f;
}

FTransform Placement(float X, float Y, float Yaw, FVector Scale, float Lift = 0.f)
{
    const FVector Up(-(HouseWoods::Height(X + 8, Y) - HouseWoods::Height(X - 8, Y)) / 16.f,
        -(HouseWoods::Height(X, Y + 8) - HouseWoods::Height(X, Y - 8)) / 16.f, 1.f);
    return FTransform(FRotationMatrix::MakeFromZX(Up.GetSafeNormal(), FRotator(0, Yaw, 0).Vector()).ToQuat(), OnGround(X, Y, Lift), Scale);
}

void TwoSidedLeaf(FHallMesh& Mesh, FName Key, FVector Root, FVector Tip, FVector Side, float Curl)
{
    const FVector Mid = FMath::Lerp(Root, Tip, .52f) + FVector(0, 0, Curl);
    const FVector Left = Mid - Side, Right = Mid + Side;
    const FVector Up = FVector::CrossProduct(Tip - Root, Side).GetSafeNormal();
    Mesh.Quad(Key, Root, Left, Tip, Mid, Up, DetailUV);
    Mesh.Quad(Key, Root, Mid, Tip, Right, Up, DetailUV);
    Mesh.Quad(Key, Root, Left, Tip, Mid, -Up, DetailUV);
    Mesh.Quad(Key, Root, Mid, Tip, Right, -Up, DetailUV);
}

void Fern(FHallMesh& Mesh)
{
    FRandomStream ShapeRng(FloorSeed + 19);
    for (int32 Frond = 0; Frond < 7; ++Frond)
    {
        const float Angle = Frond * 2.39996f;
        const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0), Side(-Direction.Y, Direction.X, 0);
        const float Length = ShapeRng.FRandRange(42.f, 76.f);
        FVector Previous(0, 0, 1);
        for (int32 Segment = 1; Segment <= 8; ++Segment)
        {
            const float Along = Segment / 8.f;
            const FVector Center = Direction * Length * Along + FVector(0, 0, FMath::Sin(Along * PI * .83f) * Length * .38f);
            Mesh.Cylinder(BarkKey, Previous, Center, .34f, 4, DetailUV);
            const float Span = Length * .19f * FMath::Sin(Along * PI);
            for (float Sign : {-1.f, 1.f})
                TwoSidedLeaf(Mesh, FernKey, Center, Center + Side * Span * Sign + Direction * 6.f - FVector(0, 0, Along * 5), Direction * 1.5f, 1.1f);
            Previous = Center;
        }
    }
}

void Bramble(FHallMesh& Mesh)
{
    for (int32 Stem = 0; Stem < 4; ++Stem)
    {
        const float Angle = Stem * 1.7f;
        const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0);
        FVector Previous = Direction * -42.f;
        for (int32 Segment = 1; Segment <= 7; ++Segment)
        {
            const float Along = Segment / 7.f;
            const FVector Point = Direction * FMath::Lerp(-42.f, 69.f, Along) + FVector(0, 0, FMath::Sin(Along * PI) * (48 + Stem * 10));
            Mesh.Cylinder(BarkKey, Previous, Point, .9f - Along * .45f, 5, DetailUV);
            const FVector Thorn = Point + FVector(-Direction.Y * 2.5, Direction.X * 2.5, 4.5);
            Mesh.Cylinder(BarkKey, Point, Thorn, .30f, 3, DetailUV);
            if (Segment == 3 || Segment == 5)
                TwoSidedLeaf(Mesh, FernKey, Point, Point + FVector(-Direction.Y, Direction.X, .16) * 12.f, Direction * 4.f, 2.f);
            Previous = Point;
        }
    }
}

void Stump(FHallMesh& Mesh)
{
    constexpr int32 Sides = 17;
    for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
    {
        const float AngleA = SideIndex * 2 * PI / Sides, AngleB = (SideIndex + 1) * 2 * PI / Sides;
        const FVector RadialA(FMath::Cos(AngleA), FMath::Sin(AngleA), 0), RadialB(FMath::Cos(AngleB), FMath::Sin(AngleB), 0);
        const float TopA = 58 + 12 * FMath::Sin(SideIndex * 2.7f), TopB = 58 + 12 * FMath::Sin((SideIndex + 1) * 2.7f);
        const FVector OuterA = RadialA * 36.f + FVector(0, 0, TopA), OuterB = RadialB * 36.f + FVector(0, 0, TopB);
        const FVector InnerA = RadialA * 23.f + FVector(0, 0, TopA - 3), InnerB = RadialB * 23.f + FVector(0, 0, TopB - 3);
        Mesh.Quad(BarkKey, RadialA * 44.f - FVector(0, 0, 7), RadialB * 44.f - FVector(0, 0, 7), OuterB, OuterA, (RadialA + RadialB).GetSafeNormal(), DetailUV);
        Mesh.Quad(EndKey, OuterA, OuterB, InnerB, InnerA, FVector::UpVector, DetailUV);
        Mesh.Quad(SoilKey, InnerA, InnerB, RadialB * 18.f + FVector(0, 0, 7), RadialA * 18.f + FVector(0, 0, 7), -(RadialA + RadialB).GetSafeNormal(), DetailUV);
        if (RadialA.Y > .1f)
            Mesh.Quad(MossKey, RadialA * 44.4f, RadialB * 44.4f, RadialB * 39.f + FVector(0, 0, 32), RadialA * 39.f + FVector(0, 0, 29), (RadialA + RadialB).GetSafeNormal(), DetailUV);
    }
    Mesh.Dome(SoilKey, FVector(0, 0, 5), FVector::UpVector, 21.f, 2.f, 12);
}

void Stone(FHallMesh& Mesh)
{
    constexpr int32 Sides = 9, Rings = 4;
    auto Point = [](int32 SideIndex, int32 RingIndex)
    {
        const float Angle = SideIndex * 2 * PI / Sides;
        const float Latitude = -.28f + RingIndex * 1.82f / Rings;
        const float Radius = (1.f + .10f * FMath::Sin(Angle * 3.f + RingIndex * .3f)) * FMath::Cos(Latitude);
        return FVector(FMath::Cos(Angle) * 58.f * Radius, FMath::Sin(Angle) * 42.f * Radius, 20.f + FMath::Sin(Latitude) * 34.f);
    };
    for (int32 RingIndex = 0; RingIndex < Rings; ++RingIndex)
        for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
        {
            const FVector A = Point(SideIndex, RingIndex), B = Point(SideIndex + 1, RingIndex), C = Point(SideIndex + 1, RingIndex + 1), D = Point(SideIndex, RingIndex + 1);
            const FVector Normal = FVector::CrossProduct(B - A, D - A).GetSafeNormal();
            Mesh.Quad((A.Y + B.Y > 5 && RingIndex > 0) ? MossKey : StoneKey, A, B, C, D, Normal, DetailUV);
        }
}

void Litter(FHallMesh& Mesh)
{
    FRandomStream ShapeRng(FloorSeed + 31);
    for (int32 LeafIndex = 0; LeafIndex < 26; ++LeafIndex)
    {
        const float Angle = ShapeRng.FRandRange(0, 2 * PI), Length = ShapeRng.FRandRange(7.f, 14.f);
        const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0), Side(-Direction.Y, Direction.X, 0);
        const FVector Center(ShapeRng.FRandRange(-55, 55), ShapeRng.FRandRange(-55, 55), ShapeRng.FRandRange(.2f, 1.2f));
        const FName Key(*FString::Printf(TEXT("EX_WoodsFloorLeaf%d"), LeafIndex % 4));
        TwoSidedLeaf(Mesh, Key, Center, Center + Direction * Length, Side * Length * .29f, ShapeRng.FRandRange(.4f, 1.7f));
    }
}

int32 Instance(AActor* Owner, UHouseBlackHallSubsystem* Look, const TCHAR* Name, const FHallMesh& Mesh, const TArray<FTransform>& Transforms, int32 Cull)
{
    if (Transforms.IsEmpty()) return 0;
    TArray<FBox> NoCollision;
    UStaticMesh* StaticMesh = HwBuildStaticMesh(Mesh, *FString::Printf(TEXT("SM_%s"), Name), Look, NoCollision);
    if (!StaticMesh) return 0;
    auto* Component = NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner, Name);
    Component->SetStaticMesh(StaticMesh);
    Component->SetMobility(EComponentMobility::Static);
    Component->SetCollisionProfileName(TEXT("NoCollision"));
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCastShadow(false);
    Component->bAffectDynamicIndirectLighting = false;
    Component->bAffectDistanceFieldLighting = false;
    Component->SetCullDistances(Cull * 3 / 4, Cull);
    Owner->AddInstanceComponent(Component);
    Component->RegisterComponent();
    for (const FTransform& Transform : Transforms) Component->AddInstance(Transform, true);
    // Registration can inherit the static mesh body profile; enforce this afterward too.
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    return Transforms.Num();
}

int32 SpatialGeometry(AActor* Owner, UHouseBlackHallSubsystem* Look, const FHallMesh& WorldMesh)
{
    // Terrain-following roots cannot share one local model. Bucket their
    // triangles into 40 m cells instead, so every HISM has a genuine nearby
    // origin and bounded local geometry rather than an identity at world zero.
    constexpr float CellSize = 4000.f;
    TMap<FIntPoint, FHallMesh> Cells;
    for (const FName MaterialKey : WorldMesh.Order)
    {
        const FHallSection& Source = WorldMesh.Sections.FindChecked(MaterialKey);
        for (int32 TriangleIndex = 0; TriangleIndex + 2 < Source.I.Num(); TriangleIndex += 3)
        {
            const int32 IA = Source.I[TriangleIndex], IB = Source.I[TriangleIndex + 1], IC = Source.I[TriangleIndex + 2];
            const FVector Center = (Source.V[IA] + Source.V[IB] + Source.V[IC]) / 3.f;
            const FIntPoint Cell(FMath::FloorToInt(Center.X / CellSize), FMath::FloorToInt(Center.Y / CellSize));
            const FVector Origin((Cell.X + .5f) * CellSize, (Cell.Y + .5f) * CellSize, 0.f);
            FHallSection& Target = Cells.FindOrAdd(Cell).Sec(MaterialKey);
            const int32 Base = Target.V.Num();
            for (const int32 VertexIndex : {IA, IB, IC})
            {
                Target.V.Add(Source.V[VertexIndex] - Origin);
                Target.N.Add(Source.N[VertexIndex]);
                Target.UV.Add(Source.UV[VertexIndex]);
                Target.T.Add(Source.T[VertexIndex]);
            }
            Target.I.Append({Base, Base + 1, Base + 2});
        }
    }
    // Stable cell iteration also stabilizes component names and review output.
    TArray<FIntPoint> CellKeys;
    Cells.GetKeys(CellKeys);
    CellKeys.Sort([](const FIntPoint& Left, const FIntPoint& Right) { return Left.Y == Right.Y ? Left.X < Right.X : Left.Y < Right.Y; });
    for (const FIntPoint Cell : CellKeys)
    {
        const FVector Origin((Cell.X + .5f) * CellSize, (Cell.Y + .5f) * CellSize, 0.f);
        const FString Name = FString::Printf(TEXT("ExteriorAppalachianFloorCell_%d_%d"), Cell.X, Cell.Y);
        Instance(Owner, Look, *Name, Cells.FindChecked(Cell), {FTransform(Origin)}, 6500);
    }
    return CellKeys.Num();
}

void GroundRibbon(FHallMesh& Mesh, FName Key, const TArray<FVector2D>& Points, float Width, bool Fade)
{
    for (int32 PointIndex = 0; PointIndex + 1 < Points.Num(); ++PointIndex)
    {
        if (Fade && PointIndex > Points.Num() / 2 && PointIndex % 3 == 0) continue;
        const FVector2D A = Points[PointIndex], B = Points[PointIndex + 1];
        const FVector2D Direction = (B - A).GetSafeNormal(), Side(-Direction.Y, Direction.X);
        const float Progress = PointIndex / float(Points.Num());
        const float HalfWidth = Width * (Fade ? 1.f - Progress * .85f : .75f + .24f * FMath::Sin(PointIndex * .63f));
        const FVector2D LeftA = A - Side * HalfWidth, RightA = A + Side * HalfWidth;
        const FVector2D LeftB = B - Side * HalfWidth, RightB = B + Side * HalfWidth;
        const FVector PA = OnGround(LeftA.X, LeftA.Y, 1.8f), PB = OnGround(LeftB.X, LeftB.Y, 1.8f), PC = OnGround(RightB.X, RightB.Y, 1.8f), PD = OnGround(RightA.X, RightA.Y, 1.8f);
        Mesh.Quad(Key, PA, PB, PC, PD, FVector::CrossProduct(PB - PA, PD - PA).GetSafeNormal(), FHallUV::World(110.f));
    }
}

// Exact lower-trunk ring profiles from the existing Nature models, sampled
// without editing their transforms. Moss hugs the north-facing actual bark.
void TrunkMoss(FHallMesh& Mesh, const FTransform& Transform, int32 Species, int32 Variant)
{
    auto Profile = [Species, Variant](int32 RingIndex, FVector& Center, float& Radius)
    {
        if (Species == 1)
        {
            const float Fraction = RingIndex / 12.f, Height = Fraction * (940.f + Variant * 26.f) * .76f;
            Center = FVector(FMath::Sin(Height * .007f + Variant) * 19, FMath::Sin(Height * .005f) * 13, Height);
            Radius = RingIndex == 0 ? 76.f : FMath::Lerp(54.f, 7.f, FMath::Pow(Fraction, .72f));
        }
        else if (Species == 0)
        {
            const float Height = RingIndex * 55.3f;
            Center = RingIndex == 0 ? FVector(0, 0, -16) : FVector(FMath::Sin(Height / 166 + Variant) * 22, FMath::Cos(Height / 138 + Variant) * 17, Height);
            Radius = RingIndex == 0 ? 41.4f : FMath::Lerp(30.f, 3.5f, FMath::Pow(RingIndex / 12.f, .8f));
        }
        else
        {
            const float Height = RingIndex * 65.f;
            Center = FVector(-10.f + FMath::Sin(Height * .004f + Variant) * 16, FMath::Sin(Height * .006f) * 13, Height);
            Radius = FMath::Lerp(19.f, 1.4f, RingIndex / 12.f);
        }
    };
    TArray<FVector> Centers;
    TArray<float> Radii;
    for (int32 RingIndex = 0; RingIndex < 13; ++RingIndex)
    {
        FVector Center; float Radius = 0;
        Profile(RingIndex, Center, Radius);
        Centers.Add(Center); Radii.Add(Radius);
    }
    const int32 Sides = Species == 1 ? 24 : Species == 0 ? 20 : 16;
    const FVector First = (Centers[1] - Centers[0]).GetSafeNormal();
    const FVector Reference = FMath::Abs(First.Z) < .9f ? FVector::UpVector : FVector::ForwardVector;
    auto RingVertex = [&Centers, &Radii, &Transform, Reference, Sides](int32 RingIndex, int32 SideIndex)
    {
        // Same leaned frame and radial ridge as HouseNature::Tube, including
        // its duplicated seam vertex. Do not approximate with horizontal rings.
        const FVector Axis = (Centers[FMath::Min(RingIndex + 1, 12)] - Centers[FMath::Max(0, RingIndex - 1)]).GetSafeNormal();
        const FVector AxisU = FVector::CrossProduct(Axis, Reference).GetSafeNormal();
        const FVector AxisV = FVector::CrossProduct(Axis, AxisU).GetSafeNormal();
        const float Angle = SideIndex * 2 * PI / Sides;
        const FVector Radial = AxisU * FMath::Cos(Angle) + AxisV * FMath::Sin(Angle);
        const float Ridge = 1.f + .028f * FMath::Sin(SideIndex * 2.7f + RingIndex * .33f);
        return Transform.TransformPosition(Centers[RingIndex] + Radial * Radii[RingIndex] * Ridge);
    };
    auto PaintFace = [&Mesh](const FVector& A, const FVector& B, const FVector& C, const FVector& TrunkCenter, float Fragment)
    {
        const FVector FaceCenter = (A + B + C) / 3.f;
        FVector Normal = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
        if (FVector::DotProduct(Normal, FaceCenter - TrunkCenter) < 0) Normal = -Normal;
        if (Normal.Y < .38f) return;
        // Barycentric insets stay on the exact original triangle, avoiding the
        // non-planar quad's hidden diagonal. Offset is in world cm, not scale.
        const FVector Lift = Normal * .20f;
        const FVector PA = FMath::Lerp(FaceCenter, A, Fragment) + Lift;
        const FVector PB = FMath::Lerp(FaceCenter, B, .91f) + Lift;
        const FVector PC = FMath::Lerp(FaceCenter, C, .87f) + Lift;
        Mesh.Quad(MossKey, PA, PB, PC, PC, Normal, DetailUV);
    };
    for (int32 Band = 0; Band < 3; ++Band)
    {
        const FVector TrunkCenter = Transform.TransformPosition((Centers[Band] + Centers[Band + 1]) * .5f);
        for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
        {
            const FVector A = RingVertex(Band, SideIndex), B = RingVertex(Band, SideIndex + 1);
            const FVector C = RingVertex(Band + 1, SideIndex + 1), D = RingVertex(Band + 1, SideIndex);
            const float Fragment = .69f + .18f * FMath::Sin(SideIndex * 2.3f + Band);
            PaintFace(A, B, C, TrunkCenter, Fragment);
            if ((SideIndex + Band) % 5 != 0) PaintFace(A, C, D, TrunkCenter, .86f);
        }
    }
}

void DryExistingFoliage(AActor* Owner, UHouseBlackHallSubsystem* Look)
{
    TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components(Owner);
    int32 ShortGrassCount = 0, TuftCount = 0;
    UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NatureFoliage.M_NatureFoliage"));
    UMaterialInstanceDynamic* Dry = nullptr;
    if (Parent)
    {
        FHallImage Blade, BladeNormal;
        Blade.Init(64, 128, FLinearColor(0, 0, 0, 0));
        TArray<float> Height; Height.Init(0.f, 64 * 128);
        for (int32 PixelY = 0; PixelY < 128; ++PixelY)
            for (int32 PixelX = 0; PixelX < 64; ++PixelX)
            {
                const float Along = PixelY / 127.f, HalfWidth = 23.f * FMath::Pow(Along, .55f);
                const float Distance = FMath::Abs(PixelX - 31.5f);
                const float Alpha = FMath::Clamp(HalfWidth - Distance, 0.f, 1.f);
                const float Vein = Distance < 1.5f ? .08f : 0.f;
                Blade.At(PixelX, PixelY) = FLinearColor(.17f + Vein, .125f + Vein * .55f, .060f, Alpha);
                Height[PixelY * 64 + PixelX] = Alpha * (.1f + Vein);
            }
        HallHeightToNormal(Height, 64, 128, .5f, false, BladeNormal);
        UTexture2D* Base = HouseStreetScene::MakeTexture(Blade, true, false, TEXT("T_WoodsFloorDryBlade"), false);
        UTexture2D* Normal = HallMakeTexture(BladeNormal, false, true, TEXT("T_WoodsFloorDryBladeN"), false);
        Look->Textures.Add(TEXT("T_WoodsFloorDryBlade"), Base); Look->Textures.Add(TEXT("T_WoodsFloorDryBladeN"), Normal);
        Dry = UMaterialInstanceDynamic::Create(Parent, Look);
        Dry->SetTextureParameterValue(TEXT("BaseMap"), Base); Dry->SetTextureParameterValue(TEXT("NormalMap"), Normal);
        Dry->SetScalarParameterValue(TEXT("WindStrength"), .50f); Dry->SetScalarParameterValue(TEXT("Transmission"), .08f);
        Look->Materials.Add(TEXT("EX_WoodsFloorDryBlade"), Dry);
    }
    for (auto* Component : Components)
    {
        const FString Name = Component->GetName();
        if (Name.StartsWith(TEXT("ExteriorWoodsGrass")))
        {
            if (Dry)
                for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot) Component->SetMaterial(Slot, Dry);
            // Original instance locations/counts and the old RNG are untouched.
            // Most October grass is flattened into leaf litter; rare taller
            // clumps are clustered by position instead of a uniform straw field.
            TArray<FTransform> GrassTransforms;
            GrassTransforms.Reserve(Component->GetInstanceCount());
            for (int32 GrassIndex = 0; GrassIndex < Component->GetInstanceCount(); ++GrassIndex)
            {
                FTransform GrassTransform;
                Component->GetInstanceTransform(GrassIndex, GrassTransform, true);
                const FVector Position = GrassTransform.GetLocation();
                const float Patch = HallFbm(Position.X * .0023f, Position.Y * .0023f, 2, FloorSeed + 83);
                const bool TallTuft = Patch > .72f && HallNoise(Position.X * .041f, Position.Y * .041f, FloorSeed + 89) > .72f;
                const float HeightFactor = TallTuft ? .55f : FMath::Lerp(.15f, .30f, FMath::Clamp(Patch, 0.f, 1.f));
                FVector Scale = GrassTransform.GetScale3D(); Scale.Z *= HeightFactor;
                GrassTransform.SetScale3D(Scale);
                GrassTransforms.Add(GrassTransform);
                ++ShortGrassCount; TuftCount += int32(TallTuft);
            }
            if (!GrassTransforms.IsEmpty()) Component->BatchUpdateInstancesTransforms(0, GrassTransforms, true, true, true);
        }
        if (Name.StartsWith(TEXT("ExteriorWoodsFlowers")))
            for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
                Component->SetMaterial(Slot, Look->Mat(Slot == 0 ? FernKey : EndKey));
    }
    UE_LOG(LogTemp, Display, TEXT("WOODS_FLOOR_GRASS shortened=%d occasionalTufts=%d ordinaryHeightFactor=0.15..0.30 tuftHeightFactor=0.55 originalCountsAndLocationsPreserved=1 originalRngUntouched=1"), ShortGrassCount, TuftCount);
}
}

void HouseWoodsFloor::CreateLook(UHouseBlackHallSubsystem* Look)
{
    if (!Look) return;
    FHallImage Color, Normal;
    HouseWoodsFloorImpl::PaintFloor(Color, Normal);
    UTexture2D* ColorTexture = HallMakeTexture(Color, true, false, TEXT("T_WoodsFloorLeaves"), true);
    UTexture2D* NormalTexture = HallMakeTexture(Normal, false, true, TEXT("T_WoodsFloorLeavesN"), true);
    Look->Textures.Add(TEXT("T_WoodsFloorLeaves"), ColorTexture);
    Look->Textures.Add(TEXT("T_WoodsFloorLeavesN"), NormalTexture);
    HouseWoodsFloorImpl::Textured(Look, TEXT("EX_WoodsGround"), ColorTexture, NormalTexture);
    HouseWoodsFloorImpl::Plain(Look, HouseWoodsFloorImpl::BarkKey, FLinearColor(.052f, .034f, .021f));
    HouseWoodsFloorImpl::Plain(Look, HouseWoodsFloorImpl::MossKey, FLinearColor(.052f, .070f, .027f));
    HouseWoodsFloorImpl::Plain(Look, HouseWoodsFloorImpl::FernKey, FLinearColor(.18f, .114f, .043f));
    HouseWoodsFloorImpl::Plain(Look, HouseWoodsFloorImpl::StoneKey, FLinearColor(.14f, .142f, .128f));
    HouseWoodsFloorImpl::Plain(Look, HouseWoodsFloorImpl::SoilKey, FLinearColor(.043f, .034f, .024f));
    HouseWoodsFloorImpl::Plain(Look, HouseWoodsFloorImpl::EndKey, FLinearColor(.128f, .085f, .035f));
    const FLinearColor LeafColors[] = {FLinearColor(.24f,.13f,.035f),FLinearColor(.15f,.084f,.028f),FLinearColor(.22f,.063f,.026f),FLinearColor(.14f,.12f,.047f)};
    for (int32 ColorIndex = 0; ColorIndex < 4; ++ColorIndex)
        HouseWoodsFloorImpl::Plain(Look, FName(*FString::Printf(TEXT("EX_WoodsFloorLeaf%d"), ColorIndex)), LeafColors[ColorIndex]);
    HouseCalderLandscape::WoodsFloorLook(Look);
}

void HouseWoodsFloor::Build(AActor* Owner, UHouseBlackHallSubsystem* Look, const TArray<FTransform>& ExistingTrees)
{
    if (!Owner || !Look) return;
    FRandomStream ScatterRng(HouseWoodsFloorImpl::FloorSeed + 101);
    TArray<FTransform> Ferns, Brambles, Stumps, Stones, LeafPatches;
    for (int32 Attempt = 0; Attempt < 2100; ++Attempt)
    {
        const float X = ScatterRng.FRandRange(-7800, 7800), Y = ScatterRng.FRandRange(4901, 18600);
        if (!HouseWoodsFloorImpl::Allowed(X, Y)) continue;
        const FTransform Transform = HouseWoodsFloorImpl::Placement(X, Y, ScatterRng.FRandRange(0, 360), FVector(ScatterRng.FRandRange(.72f, 1.25f)), .4f);
        LeafPatches.Add(Transform);
        if (Attempt % 3 == 0) Ferns.Add(Transform);
        if (Attempt % 13 == 0) Brambles.Add(Transform);
        if (Attempt % 83 == 0) Stumps.Add(Transform);
        if (Attempt % 37 == 0)
        {
            // Preserve a world-north moss face; vary scale/shape rather than yaw.
            const FVector RockScale(ScatterRng.FRandRange(1.0f, 2.9f), ScatterRng.FRandRange(.8f, 1.8f), ScatterRng.FRandRange(.6f, 1.25f));
            FTransform RockTransform = HouseWoodsFloorImpl::Placement(X, Y, 0.f, RockScale, .4f);
            Stones.Add(RockTransform);
        }
    }
    FHallMesh TerrainDetails;
    TArray<FVector2D> Creek;
    for (int32 Sample = 0; Sample < 39; ++Sample)
    {
        const float Y = 6960.f + Sample * 57.f;
        const float X = -4255.f + FMath::Sin((Y - 7000.f) * .0027f) * 155.f + FMath::Sin(Y * .006f) * 37.f;
        Creek.Add(FVector2D(X, Y));
        for (int32 Pebble = 0; Pebble < 7; ++Pebble)
        {
            const float StoneX = X + ScatterRng.FRandRange(-83, 83), StoneY = Y + ScatterRng.FRandRange(-32, 32);
            ScatterRng.FRandRange(0, 360); // Retain this stream's old draw count while keeping moss north.
            Stones.Add(HouseWoodsFloorImpl::Placement(StoneX, StoneY, 0.f, FVector(ScatterRng.FRandRange(.12f, .36f), ScatterRng.FRandRange(.13f, .31f), ScatterRng.FRandRange(.09f, .22f)), -.8f));
        }
    }
    HouseWoodsFloorImpl::GroundRibbon(TerrainDetails, HouseWoodsFloorImpl::SoilKey, Creek, 77.f, false);
    TArray<FVector2D> DeerTrack;
    for (int32 Sample = 0; Sample < 35; ++Sample)
    {
        const float Along = Sample / 34.f;
        DeerTrack.Add(FVector2D(-3530.f - Along * 2000.f, 6640.f + Along * 2200.f + FMath::Sin(Along * PI * 2) * 165.f));
    }
    HouseWoodsFloorImpl::GroundRibbon(TerrainDetails, HouseWoodsFloorImpl::EndKey, DeerTrack, 24.f, true);
    int32 Roots = 0;
    for (const FTransform& TreeTransform : ExistingTrees)
    {
        const FVector Position = TreeTransform.GetLocation();
        if (!HouseWoodsFloorImpl::Allowed(Position.X, Position.Y, 430.f)) continue;
        if (Roots >= 130) break;
        ++Roots;
        for (int32 RootIndex = 0; RootIndex < 4; ++RootIndex)
        {
            const float Angle = RootIndex * HALF_PI + ScatterRng.FRandRange(-.3f, .3f), Length = ScatterRng.FRandRange(100.f, 190.f);
            const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
            FVector Previous = HouseWoodsFloorImpl::OnGround(Position.X + Direction.X * 28, Position.Y + Direction.Y * 28, 4);
            for (int32 Segment = 1; Segment < 6; ++Segment)
            {
                const float Along = Segment / 5.f, Radius = FMath::Lerp(8.f, .7f, Along);
                const float X = Position.X + Direction.X * FMath::Lerp(28.f, Length, Along), Y = Position.Y + Direction.Y * FMath::Lerp(28.f, Length, Along);
                const FVector Next = HouseWoodsFloorImpl::OnGround(X, Y, Radius * .37f);
                TerrainDetails.Cylinder(HouseWoodsFloorImpl::BarkKey, Previous, Next, Radius, 6, HouseWoodsFloorImpl::DetailUV);
                Previous = Next;
            }
        }
    }
    int32 MossTrunks = 0;
    TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> ExistingParts(Owner);
    for (auto* Component : ExistingParts)
    {
        const FString Name = Component->GetName();
        if (!Name.StartsWith(TEXT("ExteriorTree"))) continue;
        const int32 Species = Name.Contains(TEXT("Oak")) ? 1 : Name.Contains(TEXT("Birch")) ? 2 : 0;
        const int32 Variant = FCString::Atoi(*Name.Right(1));
        for (int32 TreeIndex = 0; TreeIndex < Component->GetInstanceCount(); ++TreeIndex)
        {
            FTransform TreeTransform; Component->GetInstanceTransform(TreeIndex, TreeTransform, true);
            const FVector Position = TreeTransform.GetLocation();
            if (!HouseWoodsFloorImpl::Allowed(Position.X, Position.Y, 320.f)) continue;
            HouseWoodsFloorImpl::TrunkMoss(TerrainDetails, TreeTransform, Species, Variant); ++MossTrunks;
        }
    }
    FHallMesh FernMesh, BrambleMesh, StumpMesh, StoneMesh, LeafMesh;
    HouseWoodsFloorImpl::Fern(FernMesh); HouseWoodsFloorImpl::Bramble(BrambleMesh); HouseWoodsFloorImpl::Stump(StumpMesh); HouseWoodsFloorImpl::Stone(StoneMesh); HouseWoodsFloorImpl::Litter(LeafMesh);
    HouseWoodsFloorImpl::Instance(Owner, Look, TEXT("ExteriorAppalachianFerns"), FernMesh, Ferns, 4500);
    HouseWoodsFloorImpl::Instance(Owner, Look, TEXT("ExteriorAppalachianBrambles"), BrambleMesh, Brambles, 5200);
    HouseWoodsFloorImpl::Instance(Owner, Look, TEXT("ExteriorAppalachianStumps"), StumpMesh, Stumps, 6500);
    HouseWoodsFloorImpl::Instance(Owner, Look, TEXT("ExteriorAppalachianOutcrops"), StoneMesh, Stones, 6000);
    HouseWoodsFloorImpl::Instance(Owner, Look, TEXT("ExteriorAppalachianLeafLitter"), LeafMesh, LeafPatches, 2400);
    const int32 FloorCells = HouseWoodsFloorImpl::SpatialGeometry(Owner, Look, TerrainDetails);
    HouseWoodsFloorImpl::DryExistingFoliage(Owner, Look);
    HouseCalderLandscape::WoodsScannedProps(Owner);
    UE_LOG(LogTemp, Display, TEXT("WOODS_FLOOR procedural=1 seed=%d ferns=%d brambles=%d stumps=%d stones=%d leafPatches=%d rootedTrees=%d northMossTrunks=%d creekPoints=%d deerTrackPoints=%d spatialFloorCells=%d collision=0 shadows=0 newLights=0 terrainChanged=0"),
        HouseWoodsFloorImpl::FloorSeed, Ferns.Num(), Brambles.Num(), Stumps.Num(), Stones.Num(), LeafPatches.Num(), Roots, MossTrunks, Creek.Num(), DeerTrack.Num(), FloorCells);
}
