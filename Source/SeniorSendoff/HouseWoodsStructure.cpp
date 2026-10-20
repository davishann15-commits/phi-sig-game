#include "HouseWoodsStructure.h"
#include "HouseCalderLandscape.h"
#include "HouseCalderTrees.h"
#include "HouseWoodsDetail.h"
#include "HouseWoodsPass.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"

namespace HouseWoodsStructureImpl
{
constexpr int SpeciesCount = 5;
constexpr int TextureSize = 256;
constexpr int ScatterSeed = 2026101307;
const TCHAR* SpeciesNames[SpeciesCount] = {TEXT("Oak"), TEXT("Hickory"), TEXT("Maple"), TEXT("TulipPoplar"), TEXT("Hemlock")};
const float Heights[SpeciesCount] = {1580.f, 1790.f, 1470.f, 2020.f, 1770.f};
const float Crowns[SpeciesCount] = {485.f, 385.f, 430.f, 350.f, 330.f};
const float TrunkRadii[SpeciesCount] = {43.f, 26.f, 30.f, 33.f, 29.f};

FName BarkKey(int Species)
{
    return FName(*FString::Printf(TEXT("EX_WoodsPassBark%d"), Species));
}

FName LeafKey(int Species)
{
    return FName(*FString::Printf(TEXT("EX_WoodsPassLeaf%d"), Species));
}

FName TwigKey(int Species)
{
    return FName(*FString::Printf(TEXT("EX_WoodsPassTwig%d"), Species));
}

UTexture2D* MaskTexture(UHouseBlackHallSubsystem* Look, FName Name, const FHallImage& Paint)
{
    // Alpha-weighted, coverage-preserving mips keep distant thin branches from
    // turning into opaque sheets. Every pixel is drawn below, not imported.
    UTexture2D* Texture = UTexture2D::CreateTransient(Paint.W, Paint.H, PF_B8G8R8A8,
        MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), Name));
    if (!Texture) return nullptr;
    Texture->SRGB = true;
    Texture->CompressionSettings = TC_Default;
    Texture->AddressX = Texture->AddressY = TA_Clamp;
    Texture->Filter = TF_Trilinear;
    Texture->LODGroup = TEXTUREGROUP_World;
    FTexturePlatformData* Platform = Texture->GetPlatformData();
    TArray<FLinearColor> Pixels = Paint.Px;
    int Width = Paint.W, Height = Paint.H;
    float Coverage = 0.f;
    for (const FLinearColor& Pixel : Pixels) Coverage += Pixel.A >= .32f ? 1.f : 0.f;
    Coverage /= Pixels.Num();
    for (int MipIndex = 0;; ++MipIndex)
    {
        float AlphaScale = 1.f;
        if (MipIndex > 0 && Width * Height > 16)
        {
            float Low = .08f, High = 4.f;
            for (int Search = 0; Search < 7; ++Search)
            {
                const float Middle = (Low + High) * .5f;
                int Covered = 0;
                for (const FLinearColor& Pixel : Pixels) Covered += Pixel.A * Middle >= .32f ? 1 : 0;
                if (float(Covered) / Pixels.Num() < Coverage) Low = Middle; else High = Middle;
            }
            AlphaScale = (Low + High) * .5f;
        }
        TArray<FColor> Bytes;
        Bytes.Reserve(Pixels.Num());
        for (FLinearColor Pixel : Pixels)
        {
            Pixel.A = FMath::Clamp(Pixel.A * AlphaScale, 0.f, 1.f);
            Bytes.Add(Pixel.ToFColor(true));
        }
        FTexture2DMipMap* Mip = MipIndex == 0 ? &Platform->Mips[0] : new FTexture2DMipMap(Width, Height, 1);
        if (MipIndex > 0) Platform->Mips.Add(Mip);
        Mip->SizeX = Width;
        Mip->SizeY = Height;
        Mip->BulkData.Lock(LOCK_READ_WRITE);
        void* Data = Mip->BulkData.Realloc(int64(Width) * Height * 4);
        FMemory::Memcpy(Data, Bytes.GetData(), int64(Width) * Height * 4);
        Mip->BulkData.Unlock();
        if (Width == 1 && Height == 1) break;
        const int NextWidth = FMath::Max(1, Width / 2), NextHeight = FMath::Max(1, Height / 2);
        TArray<FLinearColor> Reduced;
        Reduced.SetNumUninitialized(NextWidth * NextHeight);
        for (int Row = 0; Row < NextHeight; ++Row) for (int Column = 0; Column < NextWidth; ++Column)
        {
            FLinearColor Average(0, 0, 0, 0);
            float Alpha = 0.f;
            for (int DY = 0; DY < 2; ++DY) for (int DX = 0; DX < 2; ++DX)
            {
                const FLinearColor& Sample = Pixels[FMath::Min(Height - 1, Row * 2 + DY) * Width + FMath::Min(Width - 1, Column * 2 + DX)];
                Average.R += Sample.R * Sample.A;
                Average.G += Sample.G * Sample.A;
                Average.B += Sample.B * Sample.A;
                Alpha += Sample.A;
            }
            if (Alpha > .00001f) { Average.R /= Alpha; Average.G /= Alpha; Average.B /= Alpha; }
            Average.A = Alpha * .25f;
            Reduced[Row * NextWidth + Column] = Average;
        }
        Pixels = MoveTemp(Reduced);
        Width = NextWidth;
        Height = NextHeight;
    }
    Texture->UpdateResource();
    Look->Textures.Add(Name, Texture);
    return Texture;
}

FLinearColor AutumnColour(int Species)
{
    if (Species == 0) return FLinearColor(.24f, .097f, .025f); // leathery russet oak
    if (Species == 1) return FLinearColor(.37f, .235f, .041f); // subdued hickory gold
    if (Species == 2) return FLinearColor(.30f, .061f, .022f); // red maple remnants
    if (Species == 3) return FLinearColor(.30f, .235f, .058f); // tulip poplar yellow
    if (Species == 4) return FLinearColor(.028f, .055f, .035f); // cool hemlock shade
    if (Species == 5) return FLinearColor(.25f, .066f, .040f); // dogwood wine red
    return FLinearColor(.040f, .075f, .032f); // persistent mountain laurel
}

void PaintLeaf(FHallImage& Paint, const FVector2D& Base, const FVector2D& Tip, float Width, int Species, FLinearColor Colour)
{
    const FVector2D Axis = Tip - Base, Cross(-Axis.Y, Axis.X);
    const FVector2D Side = Cross.GetSafeNormal();
    auto Point = [&](float U, float V) { return Base + Side * Width * U + Axis * V; };
    TArray<FVector2D> Outline;
    if (Species == 0)
    {
        const FVector2D Lobes[] = {{0,0},{-.18f,.15f},{-.62f,.19f},{-.82f,.30f},{-.42f,.39f},{-1,.48f},{-.88f,.61f},{-.43f,.65f},{-.66f,.79f},{-.24f,.88f},{0,1},
            {.24f,.88f},{.66f,.79f},{.43f,.65f},{.88f,.61f},{1,.48f},{.42f,.39f},{.82f,.30f},{.62f,.19f},{.18f,.15f}};
        for (const FVector2D& Lobe : Lobes) Outline.Add(Point(Lobe.X, Lobe.Y));
    }
    else if (Species == 2)
    {
        const FVector2D Lobes[] = {{0,0},{-.24f,.22f},{-.84f,.19f},{-.52f,.40f},{-1,.48f},{-.55f,.63f},{-.61f,.86f},{-.17f,.77f},{0,1},
            {.17f,.77f},{.61f,.86f},{.55f,.63f},{1,.48f},{.52f,.40f},{.84f,.19f},{.24f,.22f}};
        for (const FVector2D& Lobe : Lobes) Outline.Add(Point(Lobe.X, Lobe.Y));
    }
    else if (Species == 3)
    {
        // Distinctive clipped tulip-poplar apex and four broad lobes.
        const FVector2D Lobes[] = {{0,0},{-.4f,.18f},{-.88f,.34f},{-.62f,.53f},{-1,.86f},{-.47f,.92f},{0,.79f},{.47f,.92f},{1,.86f},{.62f,.53f},{.88f,.34f},{.4f,.18f}};
        for (const FVector2D& Lobe : Lobes) Outline.Add(Point(Lobe.X, Lobe.Y));
    }
    else
    {
        for (int Edge = 0; Edge < 24; ++Edge)
        {
            const float Angle = Edge * 2 * PI / 24;
            Outline.Add(Point(FMath::Sin(Angle) * (Species == 1 && Edge % 2 ? .9f : 1.f), (1 - FMath::Cos(Angle)) * .5f));
        }
    }
    Paint.FillPolygon(Outline, Colour);
    FLinearColor Vein = Colour * 1.23f;
    Vein.A = 1.f;
    Paint.Stroke({Base, Tip}, .62f, Vein);
    for (int Rib = 1; Rib < 5; ++Rib) for (float Direction : {-1.f, 1.f})
        Paint.Stroke({Point(0, Rib * .15f), Point(Direction * .68f, Rib * .15f + .14f)}, .4f, Vein);
}

void PaintSpray(FHallImage& Paint, int Species)
{
    const FLinearColor Colour = AutumnColour(Species);
    Paint.Init(TextureSize, TextureSize, FLinearColor(Colour.R, Colour.G, Colour.B, 0));
    FRandomStream Random(26371 + Species * 131);
    const FLinearColor Twig(.075f, .044f, .023f);
    Paint.Stroke({{126, 246}, {137, 178}, {118, 93}, {139, 16}}, 1.5f, Twig);
    if (Species == 4)
    {
        for (int BranchIndex = 0; BranchIndex < 10; ++BranchIndex) for (float Direction : {-1.f, 1.f})
        {
            const FVector2D Start(128, 226 - BranchIndex * 20), End = Start + FVector2D(Direction * (88 - BranchIndex * 3), -31);
            Paint.Stroke({Start, End}, 1.f, Twig);
            for (int NeedleIndex = 1; NeedleIndex < 15; ++NeedleIndex)
            {
                const FVector2D Anchor = FMath::Lerp(Start, End, NeedleIndex / 15.f);
                for (float NeedleSide : {-1.f, 1.f}) Paint.Stroke({Anchor, Anchor + FVector2D(Direction * 9, NeedleSide * Random.FRandRange(6, 12))}, 1.4f, Colour * Random.FRandRange(.8f, 1.25f));
            }
        }
    }
    else
    {
        const int Pairs = Species == 1 ? 6 : 4;
        for (int PairIndex = 0; PairIndex < Pairs; ++PairIndex) for (float Direction : {-1.f, 1.f})
        {
            const FVector2D Start(128 + FMath::Sin(PairIndex * 2.1f) * 9, 217 - PairIndex * 180.f / Pairs);
            const FVector2D End = Start + FVector2D(Direction * Random.FRandRange(52, 69), Random.FRandRange(-26, -9));
            Paint.Stroke({Start, End}, .85f, Twig);
            FLinearColor Tone = Colour * Random.FRandRange(.73f, 1.25f);
            Tone.A = 1.f;
            PaintLeaf(Paint, End - FVector2D(Direction * 9, 1), End + FVector2D(Direction * 20, -31), Species == 1 ? 12 : Species == 6 ? 11 : 22, Species, Tone);
        }
        PaintLeaf(Paint, {132, 50}, {137, 9}, Species == 6 ? 13.f : 22.f, Species, Colour);
    }
    for (int Row = 0; Row < TextureSize; ++Row) for (int Column = 0; Column < TextureSize; ++Column)
    {
        FLinearColor& Pixel = Paint.At(Column, Row);
        const float Alpha = Pixel.A;
        const float Grain = HallNoise(Column * .77f, Row * .77f, 7101 + Species);
        Pixel *= .85f + Grain * .29f;
        Pixel.A = Alpha;
    }
}

void CreateLook(UHouseBlackHallSubsystem* Look)
{
    if (Look->Materials.Contains(BarkKey(0))) return;
    UMaterialInterface* BarkMaster = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured.M_NaturePropTextured"));
    UMaterialInterface* LeafMaster = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NatureFoliage.M_NatureFoliage"));
    if (!BarkMaster || !LeafMaster) return;
    for (int Species = 0; Species < SpeciesCount; ++Species)
    {
        FHallImage Base, Normal;
        Base.Init(TextureSize, TextureSize, FLinearColor::Black);
        TArray<float> Relief;
        Relief.SetNum(TextureSize * TextureSize);
        for (int Row = 0; Row < TextureSize; ++Row) for (int Column = 0; Column < TextureSize; ++Column)
        {
            const float Warp = HallFbm(Column * .025f, Row * .034f, 3, 8101 + Species) * 14;
            const float Furrow = FMath::Pow(FMath::Max(0.f, FMath::Sin((Column + Warp) * (Species == 1 ? .51f : .30f))), 7.f);
            const float Grain = HallFbm(Column * .22f, Row * .055f, 3, 8149 + Species);
            const float Shag = Species == 1 ? FMath::SmoothStep(.53f, .77f, HallNoise(Column * .091f, Row * .015f, 8181)) * .085f : 0.f;
            const float Value = .09f + Grain * .13f - Furrow * .065f + Shag;
            Base.At(Column, Row) = FLinearColor(Value * .93f, Value * .85f, Value * .73f);
            Relief[Row * TextureSize + Column] = Grain * .17f - Furrow * .29f + Shag * 3;
        }
        HallHeightToNormal(Relief, TextureSize, TextureSize, 2.9f, true, Normal);
        const FName TextureKey(*FString::Printf(TEXT("T_WoodsPassBark%d"), Species));
        const FName NormalKey(*FString::Printf(TEXT("T_WoodsPassBark%dN"), Species));
        UTexture2D* BaseTexture = HallMakeTexture(Base, true, false, *TextureKey.ToString());
        UTexture2D* NormalTexture = HallMakeTexture(Normal, false, true, *NormalKey.ToString());
        Look->Textures.Add(TextureKey, BaseTexture);
        Look->Textures.Add(NormalKey, NormalTexture);
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BarkMaster, Look);
        Material->SetTextureParameterValue(TEXT("BaseMap"), BaseTexture);
        Material->SetTextureParameterValue(TEXT("NormalMap"), NormalTexture);
        Material->SetVectorParameterValue(TEXT("Tint"), FLinearColor::White);
        Material->SetVectorParameterValue(TEXT("Emission"), FLinearColor::Black);
        Material->SetScalarParameterValue(TEXT("UVScale"), 1.f);
        Material->SetScalarParameterValue(TEXT("Roughness"), .98f);
        Material->SetScalarParameterValue(TEXT("NormalStrength"), .92f);
        Look->Materials.Add(BarkKey(Species), Material);
        if (HouseWoodsPass::Stage() >= 3)
        {
            // The saved foliage master is only a shader here: both texture maps
            // are the same original CPU-painted bark used on the fixed trunks.
            // Restrict WPO to terminal twigs whose parent branch does not move.
            UMaterialInstanceDynamic* Twig = UMaterialInstanceDynamic::Create(LeafMaster, Look);
            Twig->SetTextureParameterValue(TEXT("BaseMap"), BaseTexture);
            Twig->SetTextureParameterValue(TEXT("NormalMap"), NormalTexture);
            Twig->SetVectorParameterValue(TEXT("Tint"), FLinearColor::White);
            Twig->SetScalarParameterValue(TEXT("Transmission"), 0.f);
            Twig->SetScalarParameterValue(TEXT("Roughness"), .98f);
            Twig->SetScalarParameterValue(TEXT("Specular"), .14f);
            Twig->SetScalarParameterValue(TEXT("WindStrength"), .60f);
            Look->Materials.Add(TwigKey(Species), Twig);
        }
    }
    for (int Species = 0; Species < 7; ++Species)
    {
        FHallImage Spray, Normal;
        PaintSpray(Spray, Species);
        const FName TextureKey(*FString::Printf(TEXT("T_WoodsPassLeaf%d"), Species));
        const FName NormalKey(*FString::Printf(TEXT("T_WoodsPassLeaf%dN"), Species));
        UTexture2D* Texture = MaskTexture(Look, TextureKey, Spray);
        TArray<float> Relief;
        Relief.Reserve(Spray.Px.Num());
        for (const FLinearColor& Pixel : Spray.Px) Relief.Add(Pixel.A * Pixel.GetLuminance() * .22f);
        HallHeightToNormal(Relief, TextureSize, TextureSize, .5f, false, Normal);
        UTexture2D* NormalTexture = HallMakeTexture(Normal, false, true, *NormalKey.ToString(), false);
        Look->Textures.Add(NormalKey, NormalTexture);
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(LeafMaster, Look);
        Material->SetTextureParameterValue(TEXT("BaseMap"), Texture);
        Material->SetTextureParameterValue(TEXT("NormalMap"), NormalTexture);
        Material->SetVectorParameterValue(TEXT("Tint"), FLinearColor::White);
        Material->SetScalarParameterValue(TEXT("WindStrength"), Species == 4 ? .48f : Species == 6 ? .6f : 1.2f);
        Material->SetScalarParameterValue(TEXT("Transmission"), Species == 4 || Species == 6 ? .22f : .38f);
        Look->Materials.Add(LeafKey(Species), Material);
    }
    HouseCalderLandscape::WoodsBarkLook(Look);
}

void Limb(FHallMesh& Mesh, FName Material, const FVector& Start, const FVector& Bend, const FVector& End,
    float BaseRadius, float TipRadius, int Sides, int Segments, bool FixedRootWind = false)
{
    FHallSection& Section = Mesh.Sec(Material);
    const int FirstVertex = Section.V.Num();
    float Along = 0.f;
    FVector Previous = Start;
    for (int Ring = 0; Ring <= Segments; ++Ring)
    {
        const float Fraction = float(Ring) / Segments, Inverse = 1 - Fraction;
        const FVector Center = Start * Inverse * Inverse + Bend * (2 * Inverse * Fraction) + End * Fraction * Fraction;
        const FVector Axis = ((Bend - Start) * Inverse + (End - Bend) * Fraction).GetSafeNormal();
        const FVector Cross = FVector::CrossProduct(Axis, FMath::Abs(Axis.Z) > .9f ? FVector::ForwardVector : FVector::UpVector).GetSafeNormal();
        const FVector Around = FVector::CrossProduct(Axis, Cross).GetSafeNormal();
        const float Radius = FMath::Lerp(BaseRadius, TipRadius, Fraction);
        Along += (Center - Previous).Size() / 145.f;
        Previous = Center;
        for (int Side = 0; Side <= Sides; ++Side)
        {
            const float Angle = Side * 2 * PI / Sides;
            const FVector Normal = Cross * FMath::Cos(Angle) + Around * FMath::Sin(Angle);
            Section.V.Add(Center + Normal * Radius * (1 + .036f * FMath::Sin(Side * 2.7f + Ring * .4f)));
            Section.N.Add(Normal);
            // M_NatureFoliage uses square(saturate(1-UV.y)) for tip motion.
            // The root ring has UV.y=1 exactly: no gap at the static attachment.
            Section.UV.Add({float(Side) / Sides * FMath::Max(1.f, BaseRadius * .10f), FixedRootWind ? 1.f - Fraction : -Along});
            Section.T.Add(FProcMeshTangent((Around * FMath::Cos(Angle) - Cross * FMath::Sin(Angle)).GetSafeNormal(), false));
        }
    }
    for (int Ring = 0; Ring < Segments; ++Ring) for (int Side = 0; Side < Sides; ++Side)
    {
        const int A = FirstVertex + Ring * (Sides + 1) + Side, B = A + 1, C = B + Sides + 1, D = A + Sides + 1;
        if (FVector::DotProduct(FVector::CrossProduct(Section.V[B] - Section.V[A], Section.V[C] - Section.V[A]), Section.N[A]) < 0)
            Section.I.Append({A, B, C, A, C, D});
        else Section.I.Append({A, C, B, A, D, C});
    }
}

void FoliageSpray(FHallMesh& Mesh, int Species, FVector Center, FVector Along, float Size, float Rotation)
{
    Along.Normalize();
    FVector Across = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal();
    if (Across.IsNearlyZero()) Across = FVector::ForwardVector;
    Across = Across.RotateAngleAxis(Rotation, Along);
    const FVector Horizontal = Across * Size, Vertical = Along * Size;
    FVector Face = FVector::CrossProduct(Horizontal, Vertical).GetSafeNormal();
    if (Face.Z < 0) Face = -Face;
    const FVector Normal = (Face * .65f + FVector::UpVector * .6f).GetSafeNormal();
    const FVector Tangent = (Across - Normal * FVector::DotProduct(Across, Normal)).GetSafeNormal();
    const FVector Fold = Center + Face * Size * .12f;
    Mesh.QuadUV(LeafKey(Species), Center - Horizontal - Vertical, Center + Horizontal - Vertical,
        Fold + Horizontal, Fold - Horizontal, Normal, {0,1}, {1,1}, {1,.5f}, {0,.5f}, Tangent);
    Mesh.QuadUV(LeafKey(Species), Fold - Horizontal, Fold + Horizontal,
        Center + Horizontal + Vertical, Center - Horizontal + Vertical, Normal, {0,.5f}, {1,.5f}, {1,0}, {0,0}, Tangent);
}

void TreeMesh(FHallMesh& Mesh, int Species, int Variant, int Detail)
{
    const FName Wood = BarkKey(Species);
    const bool MovingTips = HouseWoodsPass::Stage() >= 3;
    const bool CalderCrown = HouseCalderLandscape::Enabled() && Species != 4;
    const float Height = Heights[Species], Radius = TrunkRadii[Species];
    const float LeanX = Variant == 0 ? .044f : -.032f, LeanY = Variant == 0 ? -.023f : .051f;
    auto Spine = [&](float Z)
    {
        const float Upper = FMath::SmoothStep(Height * .28f, Height * .69f, Z);
        return FVector(Z * LeanX + FMath::Sin(Z * .0028f + Variant) * 18 + Upper * (Variant ? -59.f : 73.f),
            Z * LeanY + FMath::Sin(Z * .0042f) * 14 + Upper * (Species % 2 ? 48.f : -43.f), Z);
    };
    // Hardwoods lose their single straight leader and fork before the crown.
    // Hemlock deliberately retains the more regular conical growth habit.
    const float ForkHeight = Species == 4 ? Height : Height * (Variant ? .67f : .59f);
    Limb(Mesh, Wood, FVector::ZeroVector, Spine(ForkHeight * .47f), Spine(ForkHeight), Radius * 1.25f, Species == 4 ? 1.5f : Radius * .39f,
        Detail == 0 ? 12 : Detail == 1 ? 8 : 5, Detail == 0 ? 9 : Detail == 1 ? 6 : 3);
    FVector LeaderEnds[3];
    FVector LeaderBends[3];
    if (Species != 4) for (int LeaderIndex = 0; LeaderIndex < 3; ++LeaderIndex)
    {
        const float Angle = LeaderIndex * 2.27f + Species * .58f + Variant;
        const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0);
        const FVector Side(-Direction.Y, Direction.X, 0);
        const FVector Start = Spine(ForkHeight);
        const float Rise = Height - ForkHeight;
        LeaderEnds[LeaderIndex] = Start + Direction * Crowns[Species] * (.24f + LeaderIndex * .045f)
            + FVector(0, 0, Rise * (LeaderIndex == 0 ? 1.f : LeaderIndex == 1 ? .86f : .72f));
        LeaderBends[LeaderIndex] = FMath::Lerp(Start, LeaderEnds[LeaderIndex], .51f)
            + Side * (LeaderIndex % 2 ? 46.f : -38.f) - FVector(0, 0, 21);
        Limb(Mesh, Wood, Start, LeaderBends[LeaderIndex], LeaderEnds[LeaderIndex], Radius * (.39f - LeaderIndex * .052f), 1.1f,
            Detail == 0 ? 7 : 5, Detail == 0 ? 5 : 3);
    }
    if (Detail < 2) for (int RootIndex = 0; RootIndex < 5; ++RootIndex)
    {
        const float Angle = RootIndex * 2.39996f + Variant * .6f;
        const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0);
        Limb(Mesh, Wood, Spine(68), Out * 58 + FVector(0, 0, 12), Out * (Radius * 3.1f) + FVector(0, 0, -8), Radius * .42f, 1.6f, 6, 3);
    }
    const int Branches = Species == 4 ? 14 : 11;
    for (int BranchIndex = 0; BranchIndex < Branches; ++BranchIndex)
    {
        if (Detail == 1 && BranchIndex % 3 == 1) continue;
        if (Detail == 2 && BranchIndex % 2 == 1) continue;
        FRandomStream Random(71023 + Species * 821 + Variant * 79 + BranchIndex * 311);
        const float Angle = BranchIndex * 2.39996f + Variant * .57f + Random.FRandRange(-.65f, .65f);
        const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0), Side(-Out.Y, Out.X, 0);
        const float Fraction = float(BranchIndex) / (Branches - 1);
        const float StartZ = Height * (Species == 4 ? .14f + Fraction * .77f : .32f + Fraction * .51f + Random.FRandRange(-.055f, .055f));
        FVector Start = Spine(StartZ);
        if (Species != 4 && StartZ > ForkHeight)
        {
            const int LeaderIndex = BranchIndex % 3;
            const float AlongLeader = FMath::Clamp((StartZ - ForkHeight) / float(LeaderEnds[LeaderIndex].Z - ForkHeight), 0.f, .91f);
            Start = Spine(ForkHeight) * FMath::Square(1 - AlongLeader) + LeaderBends[LeaderIndex] * (2 * AlongLeader * (1 - AlongLeader))
                + LeaderEnds[LeaderIndex] * AlongLeader * AlongLeader;
        }
        // Peak-colour hardwoods carry an irregular broad crown. The old
        // steadily narrowing whorls made every species resemble a conifer.
        const float CrownProfile = CalderCrown ?
            (.64f + .40f * FMath::Sin(PI * FMath::Pow(Fraction, .72f))) * FMath::Lerp(1.f, .72f, Fraction) :
            FMath::Lerp(.93f, .20f, FMath::Pow(Fraction, 1.32f));
        const float Width = Crowns[Species] * (Species == 4 ? 1 - Fraction * .87f : CrownProfile)
            * Random.FRandRange(.76f, 1.10f);
        const bool UprightFork = Species != 4 && BranchIndex % 3 == 1;
        const FVector End = Start + Out * Width + Side * Random.FRandRange(-72, 72)
            + FVector(0, 0, Species == 4 ? -55.f : Height * Random.FRandRange(UprightFork ? .15f : .025f, UprightFork ? .24f : .115f));
        const FVector Bend = FMath::Lerp(Start, End, Random.FRandRange(.36f, .61f))
            + Side * Random.FRandRange(-49, 49) + FVector(0, 0, Species == 4 ? 26.f : Height * Random.FRandRange(-.035f, .018f));
        Limb(Mesh, Wood, Start, Bend, End, Radius * (Species == 4 ? .22f : .40f) * (1 - Fraction * .6f), Detail == 2 ? 1.1f : .55f,
            Detail == 0 ? 7 : 5, Detail == 0 ? 4 : 2);
        const int Forks = CalderCrown ? (Detail == 0 ? 5 : Detail == 1 ? 3 : 2) : (Detail == 0 ? 4 : Detail == 1 ? 2 : 1);
        for (int ForkIndex = 0; ForkIndex < Forks; ++ForkIndex)
        {
            const float Along = CalderCrown ? .29f + ForkIndex * (.65f / FMath::Max(1, Forks - 1)) : (Detail == 2 ? .76f : .38f + ForkIndex * .18f);
            const FVector Anchor = Start * FMath::Square(1 - Along) + Bend * (2 * Along * (1 - Along)) + End * Along * Along;
            const float Direction = ForkIndex % 2 ? 1.f : -1.f;
            const FVector Tip = Anchor + Out * Width * .27f + Side * (Direction * Width * (Species == 4 ? .25f : .34f)) + FVector(0, 0, Species == 4 ? -18.f : 75.f);
            if (Detail < 2) Limb(Mesh, MovingTips ? TwigKey(Species) : Wood, Anchor,
                FMath::Lerp(Anchor, Tip, .5f) + FVector(0, 0, 16), Tip, 2.2f, .18f, 5, 2, MovingTips);
            // Late October leaves occur in small sprays at live twig tips. Bare
            // inner forks and some completely bare branch ends remain visible.
            if (Species != 4 && (BranchIndex + ForkIndex + Variant) % 8 == 0) continue;
            const int Sprays = CalderCrown ? (Detail == 0 ? 6 : Detail == 1 ? 5 : 4) : (Detail == 0 ? 4 : Detail == 1 ? 3 : 2);
            for (int SprayIndex = 0; SprayIndex < Sprays; ++SprayIndex)
            {
                const float Spread = Detail == 2 ? 145.f : Detail == 1 ? 90.f : 60.f;
                const float SprayAlong = CalderCrown ? .30f + SprayIndex * (.68f / FMath::Max(1, Sprays - 1)) : .55f + SprayIndex * .14f;
                const FVector Center = FMath::Lerp(Anchor, Tip, SprayAlong) + Side * Random.FRandRange(-Spread, Spread) + FVector(0, 0, Random.FRandRange(-35, 45));
                const FVector AlongSpray = (Tip - Anchor).GetSafeNormal() + Side * Random.FRandRange(-.8f, .8f) + FVector(0, 0, Random.FRandRange(-.45f, .4f));
                const float Size = Detail == 2 ? (Species == 4 ? 115.f : 128.f) : Detail == 1 ? 79.f : Random.FRandRange(43, 61);
                FoliageSpray(Mesh, Species, Center, AlongSpray, Size, Random.FRandRange(-65, 65));
            }
        }
    }
}

void UnderstoryMesh(FHallMesh& Mesh, bool Laurel, int Variant, int Detail)
{
    FRandomStream Random(82013 + Variant * 619 + (Laurel ? 293 : 0));
    const bool MovingTips = HouseWoodsPass::Stage() >= 3;
    const int Stems = Laurel ? 9 : 3;
    const float Height = Laurel ? 145.f : 345.f;
    for (int StemIndex = 0; StemIndex < Stems; ++StemIndex)
    {
        const float Angle = StemIndex * 2.39996f + Variant;
        const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0), Side(-Out.Y, Out.X, 0);
        const FVector Start = Out * Random.FRandRange(2, 13);
        const FVector Tip = Out * (Laurel ? Random.FRandRange(83, 137) : Random.FRandRange(48, 115)) + FVector(0, 0, Height * Random.FRandRange(.7f, 1.15f));
        Limb(Mesh, BarkKey(Laurel ? 0 : 2), Start, FMath::Lerp(Start, Tip, .5f) - FVector(0, 0, 20), Tip, Laurel ? 2.4f : 4.5f, .26f, Detail == 0 ? 6 : 4, Detail == 0 ? 4 : 2);
        const int Forks = Laurel ? 4 : 7;
        for (int ForkIndex = 0; ForkIndex < Forks; ++ForkIndex)
        {
            const float Along = .32f + ForkIndex * (Laurel ? .18f : .092f);
            const FVector Anchor = FMath::Lerp(Start, Tip, Along);
            const FVector End = Anchor + Out * (Laurel ? 27.f : 72.f) + Side * ((ForkIndex % 2 ? 1.f : -1.f) * (Laurel ? 32.f : 104.f)) + FVector(0, 0, Laurel ? 24.f : 28.f);
            if (Detail == 0) Limb(Mesh, MovingTips ? TwigKey(Laurel ? 0 : 2) : BarkKey(Laurel ? 0 : 2),
                Anchor, FMath::Lerp(Anchor, End, .5f), End, 1.1f, .12f, 4, 2, MovingTips);
            if (!Laurel && (ForkIndex + Variant) % 4 == 0) continue;
            for (int SprayIndex = 0; SprayIndex < (Detail == 0 ? 3 : 2); ++SprayIndex)
            {
                FoliageSpray(Mesh, Laurel ? 6 : 5, FMath::Lerp(Anchor, End, .55f + SprayIndex * .22f),
                    (End - Anchor).GetSafeNormal() + FVector(0, 0, .25f), Laurel ? (Detail == 0 ? 24.f : 31.f) : 35.f,
                    Random.FRandRange(-65, 65));
            }
        }
    }
}

void MeshDescription(const FHallMesh& Mesh, FMeshDescription& Description)
{
    FStaticMeshAttributes Attributes(Description);
    Attributes.Register();
    auto Positions = Attributes.GetVertexPositions();
    auto Normals = Attributes.GetVertexInstanceNormals();
    auto Tangents = Attributes.GetVertexInstanceTangents();
    auto Signs = Attributes.GetVertexInstanceBinormalSigns();
    auto UVs = Attributes.GetVertexInstanceUVs();
    UVs.SetNumChannels(1);
    auto Slots = Attributes.GetPolygonGroupMaterialSlotNames();
    for (const FName Key : Mesh.Order)
    {
        const FHallSection& Section = Mesh.Sections[Key];
        const FPolygonGroupID Group = Description.CreatePolygonGroup();
        Slots[Group] = Key;
        TArray<FVertexID> Vertices;
        Vertices.Reserve(Section.V.Num());
        for (const FVector& Position : Section.V)
        {
            const FVertexID Vertex = Description.CreateVertex();
            Positions[Vertex] = FVector3f(Position);
            Vertices.Add(Vertex);
        }
        for (int Triangle = 0; Triangle + 2 < Section.I.Num(); Triangle += 3)
        {
            TArray<FVertexInstanceID> Corners;
            for (int Corner = 0; Corner < 3; ++Corner)
            {
                const int Index = Section.I[Triangle + Corner];
                const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertices[Index]);
                Normals[Instance] = FVector3f(Section.N[Index]);
                Tangents[Instance] = FVector3f(Section.T[Index].TangentX);
                Signs[Instance] = 1;
                UVs.Set(Instance, 0, FVector2f(Section.UV[Index]));
                Corners.Add(Instance);
            }
            Description.CreatePolygon(Group, Corners);
        }
    }
}

UStaticMesh* BuildMesh(const TArray<FHallMesh>& Levels, FName Name, UHouseBlackHallSubsystem* Look, float CollisionRadius)
{
    UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UStaticMesh::StaticClass(), Name), RF_Transient);
    Mesh->NeverStream = true;
    Mesh->bAllowCPUAccess = false;
    for (const FName Key : Levels[0].Order) Mesh->GetStaticMaterials().Add(FStaticMaterial(Look->Mat(Key), Key
#if WITH_EDITORONLY_DATA
        , Key
#endif
    ));
    TArray<FMeshDescription> Descriptions;
    Descriptions.SetNum(Levels.Num());
    TArray<const FMeshDescription*> Pointers;
    for (int LevelIndex = 0; LevelIndex < Levels.Num(); ++LevelIndex)
    {
        MeshDescription(Levels[LevelIndex], Descriptions[LevelIndex]);
        Pointers.Add(&Descriptions[LevelIndex]);
    }
    UStaticMesh::FBuildMeshDescriptionsParams Parameters;
    Parameters.bBuildSimpleCollision = false;
    Parameters.bFastBuild = true;
    Parameters.bAllowCpuAccess = false;
    Parameters.bMarkPackageDirty = false;
    Parameters.bCommitMeshDescription = false;
    if (!Mesh->BuildFromMeshDescriptions(Pointers, Parameters)) return nullptr;
    if (FStaticMeshRenderData* Data = Mesh->GetRenderData())
    {
        // Broad crowns can be seen through the dusk for 150m, but only nearby
        // trees need individual twig forks. Far LOD retains the same silhouette.
        Data->ScreenSize[0].Default = 1.f;
        if (Levels.Num() > 1) Data->ScreenSize[1].Default = CollisionRadius > 0 ? .50f : .22f;
        if (Levels.Num() > 2) Data->ScreenSize[2].Default = .23f;
    }
    Mesh->CreateBodySetup();
    if (UBodySetup* Body = Mesh->GetBodySetup())
    {
        Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
        Body->AggGeom.EmptyElements();
        if (CollisionRadius > 0)
        {
            FKBoxElem Trunk;
            Trunk.Center = FVector(0, 0, 200);
            Trunk.X = Trunk.Y = CollisionRadius * 2;
            Trunk.Z = 400;
            Body->AggGeom.BoxElems.Add(Trunk);
            Body->bGenerateMirroredCollision = false;
            Body->InvalidatePhysicsData();
            Body->CreatePhysicsMeshes();
        }
    }
    return Mesh;
}

int AddBatch(AActor* Owner, UHouseBlackHallSubsystem* Look, const FString& Name, const TArray<FHallMesh>& Levels,
    const TArray<FTransform>& Transforms, float TrunkRadius, int CullDistance)
{
    if (Transforms.IsEmpty()) return 0;
    UStaticMesh* Mesh = BuildMesh(Levels, FName(*FString(TEXT("SM_" ) + Name)), Look, TrunkRadius);
    if (!Mesh) return 0;
    Mesh=HouseCalderTrees::WoodsMesh(Owner,Look,Name,Mesh,Transforms);
    UHierarchicalInstancedStaticMeshComponent* Component = NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner, FName(*Name), RF_Transient);
    Owner->AddInstanceComponent(Component);
    Component->SetupAttachment(Owner->GetRootComponent());
    Component->SetMobility(EComponentMobility::Static);
    Component->SetStaticMesh(Mesh);
    Component->SetCollisionProfileName(TrunkRadius > 0 ? UCollisionProfile::BlockAll_ProfileName : FName(TEXT("NoCollision")));
    Component->SetCollisionEnabled(TrunkRadius > 0 ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCastShadow(false);
    Component->bAffectDistanceFieldLighting = false;
    Component->bAffectDynamicIndirectLighting = false;
    Component->SetCullDistances(FMath::RoundToInt(CullDistance * .8f), CullDistance);
    Component->RegisterComponent();
    // Static-mesh registration may inherit its default body profile. Assert
    // decoration is still non-colliding after registration, as the old woods do.
    Component->SetCollisionProfileName(TrunkRadius > 0 ? UCollisionProfile::BlockAll_ProfileName : FName(TEXT("NoCollision")));
    Component->SetCollisionEnabled(TrunkRadius > 0 ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Component->AddInstances(Transforms, false, true);
    Component->BuildTreeIfOutdated(false, true);
    UE_LOG(LogTemp, Display, TEXT("WOODS_STRUCTURE_BATCH name=%s instances=%d lods=%d tris=%d/%d/%d cull=%d collision=%s"),
        *Name, Transforms.Num(), Mesh->GetRenderData()->LODResources.Num(), Mesh->GetRenderData()->LODResources[0].GetNumTriangles(),
        Mesh->GetRenderData()->LODResources.Num()>1?Mesh->GetRenderData()->LODResources[1].GetNumTriangles():0,
        Mesh->GetRenderData()->LODResources.Num()>2?Mesh->GetRenderData()->LODResources[2].GetNumTriangles():0, CullDistance, TrunkRadius > 0 ? TEXT("trunkOnly") : TEXT("none"));
    return Transforms.Num();
}

bool PositionAllowed(float X, float Y, float Radius)
{
    if (Y < 5100 || Y > 18400 || FMath::Abs(X) + Radius > 7900 || (Y < 6800 && X < -6200)) return false;
    if (HouseWoods::TrailDistance(X, Y) < 450 + Radius) return false;
    if (FVector2D((X - HouseWoods::HutX) / 1.25f, Y - HouseWoods::HutY).Size() < 1250 + Radius) return false;
    for (const HouseWoods::FHollow& Hollow : HouseWoods::Hollows())
        if (FVector2D(X - Hollow.Center.X, Y - Hollow.Center.Y).Size() < Hollow.Radius + Radius) return false;
    return true;
}
}

void HouseWoodsStructure::Build(AActor* Owner, UHouseBlackHallSubsystem* Look, const TArray<FTransform>& ExistingTrees)
{
    if (!Owner || !Look) return;
    HouseWoodsStructureImpl::CreateLook(Look);
    if (!Look->Materials.Contains(HouseWoodsStructureImpl::BarkKey(0))) return;
    TArray<FTransform> TreeBatches[HouseWoodsStructureImpl::SpeciesCount][2];
    TArray<FTransform> ShrubBatches[2][2];
    TArray<FVector> NewTrunks;
    FRandomStream Random(HouseWoodsStructureImpl::ScatterSeed);
    int OldTrees = 0;
    float LowestScale = 100, HighestScale = 0;
    // Own stream, never a draw from FExBuilder::Rng. The staggered cell size
    // creates a denser interior while preserving every original tree placement.
    for (float Row = 5110; Row < 18300; Row += 530)
        for (float Column = -7500; Column < 7600; Column += 580)
        {
            const float X = Column + Random.FRandRange(-230, 230), Y = Row + Random.FRandRange(0, 250);
            const float Depth = FMath::Clamp((Y - 5100) / 9500.f, 0.f, 1.f);
            if (Random.FRand() > FMath::Lerp(.57f, .89f, Depth)) continue;
            const float Kind = Random.FRand();
            const int Species = Kind < .31f ? 0 : Kind < .52f ? 1 : Kind < .73f ? 2 : Kind < .91f ? 3 : 4;
            const int Variant = Random.RandRange(0, 1);
            float Scale = Random.FRandRange(.64f, 1.11f);
            const bool Ancient = Species == 0 && Random.FRand() < .075f;
            if (Ancient) Scale = 1.48f;
            const float Radius = HouseWoodsStructureImpl::Crowns[Species] * Scale * 1.30f;
            if (!HouseWoodsStructureImpl::PositionAllowed(X, Y, Radius)) continue;
            const FVector Position(X, Y, HouseWoods::Height(X, Y) - 4);
            bool Overlap = false;
            for (const FTransform& Original : ExistingTrees)
                if (FVector::DistSquared2D(Position, Original.GetLocation()) < FMath::Square(210.f)) { Overlap = true; break; }
            for (const FVector& Trunk : NewTrunks)
                if (FVector::DistSquared2D(Position, Trunk) < FMath::Square(330.f)) { Overlap = true; break; }
            // Existing terrain acceptance samples must still test terrain, not a new trunk top.
            for (float ProbeX = -7000; ProbeX < 8000 && !Overlap; ProbeX += 1800)
                for (float ProbeY = 5000; ProbeY < 18500; ProbeY += 2100)
                    if (FVector2D(X - ProbeX, Y - ProbeY).Size() < 115) { Overlap = true; break; }
            if (Overlap) continue;
            NewTrunks.Add(Position);
            const float Thickness = Ancient ? 1.28f : Random.FRandRange(.76f, 1.19f);
            TreeBatches[Species][Variant].Add(FTransform(FRotator(0, Random.FRandRange(0, 360), 0), Position,
                FVector(Scale * Thickness, Scale * Thickness, Scale * Random.FRandRange(.91f, 1.10f))));
            OldTrees += Ancient ? 1 : 0;
            LowestScale = FMath::Min(LowestScale, Scale);
            HighestScale = FMath::Max(HighestScale, Scale);
        }
    // Discontinuous groups grow on banks and around openings, not a uniform
    // hedge grid. Most laurel stays below eye level, dogwoods occupy the middle.
    FRandomStream Understory(2026101329);
    for (int Attempt = 0; Attempt < 1100; ++Attempt)
    {
        const float X = Understory.FRandRange(-7580, 7580), Y = Understory.FRandRange(5120, 18150);
        const bool Laurel = Understory.FRand() < .70f;
        const float Scale = Understory.FRandRange(.64f, 1.16f);
        const float Radius = (Laurel ? 175.f : 250.f) * Scale;
        if (!HouseWoodsStructureImpl::PositionAllowed(X, Y, Radius)) continue;
        const float Patch = HallFbm(X * .0017f, Y * .0017f, 3, 7319);
        if (Patch < (Laurel ? .47f : .37f)) continue;
        const FVector Position(X, Y, HouseWoods::Height(X, Y) - 2);
        bool InTrunk = false;
        for (const FTransform& Original : ExistingTrees)
            if (FVector::DistSquared2D(Position, Original.GetLocation()) < FMath::Square(90.f + Radius * .3f)) { InTrunk = true; break; }
        for (const FVector& Trunk : NewTrunks)
            if (FVector::DistSquared2D(Position, Trunk) < FMath::Square(85.f + Radius * .3f)) { InTrunk = true; break; }
        if (InTrunk) continue;
        const int Variant = Understory.RandRange(0, 1);
        ShrubBatches[Laurel ? 1 : 0][Variant].Add(FTransform(FRotator(0, Understory.FRandRange(0, 360), 0), Position,
            FVector(Scale, Scale, Scale * Understory.FRandRange(.85f, 1.05f))));
    }
    int Trees = 0, Shrubs = 0;
    for (int Species = 0; Species < HouseWoodsStructureImpl::SpeciesCount; ++Species) for (int Variant = 0; Variant < 2; ++Variant)
    {
        TArray<FHallMesh> Levels;
        Levels.SetNum(3);
        for (int Detail = 0; Detail < 3; ++Detail) HouseWoodsStructureImpl::TreeMesh(Levels[Detail], Species, Variant, Detail);
        Trees += HouseWoodsStructureImpl::AddBatch(Owner, Look,
            FString::Printf(TEXT("ExteriorWoodsPass%s%d"), HouseWoodsStructureImpl::SpeciesNames[Species], Variant), Levels,
            TreeBatches[Species][Variant], HouseWoodsStructureImpl::TrunkRadii[Species], 22500);
    }
    for (int Kind = 0; Kind < 2; ++Kind) for (int Variant = 0; Variant < 2; ++Variant)
    {
        TArray<FHallMesh> Levels;
        Levels.SetNum(2);
        for (int Detail = 0; Detail < 2; ++Detail) HouseWoodsStructureImpl::UnderstoryMesh(Levels[Detail], Kind == 1, Variant, Detail);
        Shrubs += HouseWoodsStructureImpl::AddBatch(Owner, Look,
            FString::Printf(TEXT("ExteriorWoodsPass%s%d"), Kind == 1 ? TEXT("LaurelShrub") : TEXT("DogwoodSapling"), Variant),
            Levels, ShrubBatches[Kind][Variant], 0, Kind == 1 ? 6400 : 7800);
    }
    UE_LOG(LogTemp, Display, TEXT("WOODS_STRUCTURE trees=%d ancientOaks=%d understory=%d scale=%.2f..%.2f originalTreesUntouched=%d seed=%d provenance=originalCodeGeneratedPixelsAndMeshes newLights=0 fixedRootTwigWind=%d"),
        Trees, OldTrees, Shrubs, LowestScale, HighestScale, ExistingTrees.Num(), HouseWoodsStructureImpl::ScatterSeed, int(HouseWoodsPass::Stage() >= 3));
}

TArray<FName> HouseWoodsStructure::SwayMaterialKeys()
{
    TArray<FName> Keys;
    for (int Species = 0; Species < 7; ++Species) Keys.Add(HouseWoodsStructureImpl::LeafKey(Species));
    if (HouseWoodsPass::Stage() >= 3)
        for (int Species = 0; Species < HouseWoodsStructureImpl::SpeciesCount; ++Species) Keys.Add(HouseWoodsStructureImpl::TwigKey(Species));
    HouseCalderTrees::AppendWindKeys(Keys);
    return Keys;
}
