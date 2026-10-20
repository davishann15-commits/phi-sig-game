#include "HouseWoodsTraces.h"
#include "HouseCalderMapTrees.h"
#include "HouseWoodsDetail.h"
#include "HouseExteriorNature.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseDetail.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace HouseWoodsTracesImpl
{
constexpr int32 Seed = 2026101341;
const FName Rust(TEXT("EX_WoodsTraceRust")), Enamel(TEXT("EX_WoodsTraceEnamel"));
const FName Wood(TEXT("EX_WoodsTraceWood")), Cloth(TEXT("EX_WoodsTraceCloth"));
const FName Rope(TEXT("EX_WoodsTraceRope")), Ash(TEXT("EX_WoodsTraceAsh"));
const FName Sign(TEXT("EX_WoodsTraceSign")), Dark(TEXT("EX_WoodsTraceDark"));
const FName Metal(TEXT("EX_WoodsTraceMetal")), Cut(TEXT("EX_WoodsTraceCut"));
const FHallUV UV = FHallUV::World(70.f);

struct FTree
{
    UHierarchicalInstancedStaticMeshComponent* Component = nullptr;
    FTransform Transform;
    HouseNature::ETreeKind Kind = HouseNature::ETreeKind::Oak;
    int32 Variant = 0, Instance = 0;
};

struct FSupport
{
    FVector Center, Axis;
    float Radius = 0;
};

FVector Ground(float X, float Y, float Raise = 0.f)
{
    return FVector(X, Y, HouseWoods::Height(X, Y) + Raise);
}

void Plain(UHouseBlackHallSubsystem* Look, FName Key, FLinearColor Tint, float Roughness, float Metallic = 0.f)
{
    auto* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropPlain.M_NaturePropPlain"));
    if (!Parent) { HallMakePlain(Look, Key, Tint, Roughness, Metallic); return; }
    auto* Material = UMaterialInstanceDynamic::Create(Parent, Look);
    Material->SetVectorParameterValue(TEXT("Tint"), Tint);
    Material->SetVectorParameterValue(TEXT("Emission"), FLinearColor::Black);
    Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
    Material->SetScalarParameterValue(TEXT("Metallic"), Metallic);
    Look->Materials.Add(Key, Material);
}

void Texture(UHouseBlackHallSubsystem* Look, FName Key, FHallImage& Paint, const TArray<float>& Relief, float Roughness, float Metallic = 0.f)
{
    FHallImage Normal;
    HallHeightToNormal(Relief, Paint.W, Paint.H, 1.8f, true, Normal);
    const FString BaseName = FString(TEXT("T_")) + Key.ToString(), NormalName = BaseName + TEXT("N");
    auto* BaseTexture = HallMakeTexture(Paint, true, false, *BaseName);
    auto* NormalTexture = HallMakeTexture(Normal, false, true, *NormalName);
    Look->Textures.Add(FName(*BaseName), BaseTexture);
    Look->Textures.Add(FName(*NormalName), NormalTexture);
    auto* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured.M_NaturePropTextured"));
    if (!Parent) { HallMakeTextured(Look, Key, BaseTexture, NormalTexture, Roughness, FLinearColor::White, .8f, Metallic); return; }
    auto* Material = UMaterialInstanceDynamic::Create(Parent, Look);
    Material->SetTextureParameterValue(TEXT("BaseMap"), BaseTexture);
    Material->SetTextureParameterValue(TEXT("NormalMap"), NormalTexture);
    Material->SetVectorParameterValue(TEXT("Tint"), FLinearColor::White);
    Material->SetVectorParameterValue(TEXT("Emission"), FLinearColor::Black);
    Material->SetScalarParameterValue(TEXT("UVScale"), 1.f);
    Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
    Material->SetScalarParameterValue(TEXT("Metallic"), Metallic);
    Material->SetScalarParameterValue(TEXT("NormalStrength"), .8f);
    Look->Materials.Add(Key, Material);
}

void MakeLook(UHouseBlackHallSubsystem* Look)
{
    if (Look->Materials.Contains(Rust)) return;
    Plain(Look, Dark, FLinearColor(.013f, .011f, .009f), .98f);
    Plain(Look, Metal, FLinearColor(.09f, .095f, .090f), .78f, .72f);
    Plain(Look, Cut, FLinearColor(.22f, .16f, .085f), .99f);
    const FName Keys[] = {Rust, Enamel, Wood, Cloth, Rope, Ash};
    for (int32 Kind = 0; Kind < 6; ++Kind)
    {
        const int32 Size = Kind < 3 ? 512 : Kind == 3 ? 256 : 128;
        FHallImage Paint; Paint.Init(Size, Size, FLinearColor::Black);
        TArray<float> Relief; Relief.SetNum(Size * Size);
        for (int32 Row = 0; Row < Size; ++Row) for (int32 Column = 0; Column < Size; ++Column)
        {
            const float Grain = HallNoise(Column * .88f, Row * .88f, Seed + Kind * 17);
            const float Cloud = HallFbm(Column * .028f, Row * .035f, 4, Seed + 37 + Kind);
            const float Pits = FMath::SmoothStep(.73f, .95f, Grain);
            FLinearColor Colour;
            float Bump = Grain * .10f + Cloud * .09f;
            if (Kind <= 1)
            {
                const float RustAmount = FMath::SmoothStep(Kind == 0 ? .20f : .47f, Kind == 0 ? .63f : .66f, Cloud);
                const FLinearColor Oxide(.17f + Grain * .095f, .047f + Grain * .035f, .015f + Grain * .012f);
                const FLinearColor PaintColour = Kind == 0 ? FLinearColor(.13f, .14f, .125f) : FLinearColor(.40f, .41f, .32f);
                Colour = FMath::Lerp(PaintColour, Oxide, RustAmount) * (.76f + Grain * .32f);
                Bump += RustAmount * .10f - Pits * .10f;
            }
            else if (Kind == 2)
            {
                const float Warp = HallFbm(Column * .026f, Row * .011f, 3, Seed + 103) * 15;
                const float Furrow = FMath::Pow(FMath::Max(0.f, FMath::Sin((Column + Warp) * .24f)), 13.f);
                const float Value = .14f + Cloud * .15f + Grain * .035f - Furrow * .065f;
                Colour = FLinearColor(Value, Value * .84f, Value * .64f);
                Bump -= Furrow * .24f;
            }
            else if (Kind == 3)
            {
                const float Weave = (Column % 3 == 0 ? .014f : 0) + (Row % 3 == 0 ? .01f : 0);
                Colour = FLinearColor(.060f + Cloud * .045f + Weave, .067f + Cloud * .040f + Weave, .052f + Cloud * .032f + Weave);
                Bump = Weave * 2 + Grain * .025f;
            }
            else if (Kind == 4)
            {
                const float Twist = .5f + .5f * FMath::Sin(Column * .48f + Row * .26f);
                Colour = FLinearColor(.15f, .125f, .083f) * (.70f + Twist * .38f + Grain * .12f);
                Bump = Twist * .15f + Grain * .035f;
            }
            else
            {
                const float Value = .038f + Cloud * .055f + Pits * .07f;
                Colour = FLinearColor(Value, Value * .96f, Value * .86f);
            }
            Paint.At(Column, Row) = Colour;
            Relief[Row * Size + Column] = Bump;
        }
        Texture(Look, Keys[Kind], Paint, Relief, Kind == 1 ? .83f : .98f, Kind < 2 ? .28f : 0.f);
    }
    FHallImage Paint; Paint.Init(512, 256, FLinearColor(.47f, .40f, .235f));
    FHallTextStyle Style; Style.Height = 51; Style.Stroke = 5.8f; Style.Spacing = .16f; Style.Wobble = .55f; Style.Seed = Seed;
    HallDrawText(Paint, TEXT("NO"), {(512 - HallTextWidth(TEXT("NO"), Style)) * .5f, 73}, Style, FLinearColor(.23f, .037f, .025f));
    Style.Height = 42; Style.Stroke = 4.7f; Style.Spacing = .15f;
    HallDrawText(Paint, TEXT("TRESPASSING"), {(512 - HallTextWidth(TEXT("TRESPASSING"), Style)) * .5f, 139}, Style, FLinearColor(.105f, .08f, .045f));
    Style.Height = 20; Style.Stroke = 2.4f;
    HallDrawText(Paint, TEXT("PRIVATE WOODS"), {(512 - HallTextWidth(TEXT("PRIVATE WOODS"), Style)) * .5f, 201}, Style, FLinearColor(.14f, .105f, .058f));
    Paint.Stroke({{13,13},{499,13},{499,243},{13,243},{13,13}}, 4, FLinearColor(.20f, .055f, .033f), .68f);
    TArray<float> Relief; Relief.SetNum(512 * 256);
    for (int32 Row = 0; Row < 256; ++Row) for (int32 Column = 0; Column < 512; ++Column)
    {
        const float Noise = HallFbm(Column * .11f, Row * .055f, 3, Seed + 147);
        const float Edge = FMath::Min(FMath::Min(Column, 511 - Column), FMath::Min(Row, 255 - Row));
        const float Wear = FMath::SmoothStep(.62f, .86f, Noise) * .84f;
        const FLinearColor Bare(.29f, .23f, .13f);
        Paint.At(Column, Row) = FMath::Lerp(Paint.At(Column, Row), Bare, Wear) * (.80f + Noise * .26f);
        if (Edge < 5 + Noise * 9) Paint.At(Column, Row) = FMath::Lerp(Paint.At(Column, Row), FLinearColor(.11f, .046f, .019f), .65f);
        Relief[Row * 512 + Column] = Noise * .06f + Wear * .06f;
    }
    Texture(Look, Sign, Paint, Relief, .96f, .10f);
}

void Beam(FHallMesh& Mesh, FName Key, FVector A, FVector B, float Radius, int32 Sides = 7)
{
    if (FVector::DistSquared(A, B) > .0001f) Mesh.Cylinder(Key, A, B, Radius, Sides, UV);
}

void Board(FHallMesh& Mesh, FVector A, FVector B, float Width, float Thickness)
{
    const FVector Delta = B - A;
    Mesh.Box(Wood, FTransform(FRotationMatrix::MakeFromZ(Delta).ToQuat(), (A + B) * .5f),
        FVector(Width * .5f, Thickness * .5f, Delta.Size() * .5f), UV);
}

void Ellipsoid(FHallMesh& Mesh, FName Key, const FVector& Center, const FVector& Radii, int32 Sides = 12, int32 Rings = 6)
{
    for (int32 Ring = 0; Ring < Rings; ++Ring) for (int32 Side = 0; Side < Sides; ++Side)
    {
        auto At = [&](int32 Row, int32 Column)
        {
            const float Latitude = -PI * .5f + PI * Row / Rings, Longitude = 2 * PI * Column / Sides;
            return Center + FVector(Radii.X * FMath::Cos(Latitude) * FMath::Cos(Longitude),
                Radii.Y * FMath::Cos(Latitude) * FMath::Sin(Longitude), Radii.Z * FMath::Sin(Latitude));
        };
        const FVector A = At(Ring, Side), B = At(Ring, Side + 1), C = At(Ring + 1, Side + 1), D = At(Ring + 1, Side);
        Mesh.Quad(Key, A, B, C, D, ((A + B + C + D) * .25f - Center).GetSafeNormal(), UV);
    }
}

int32 Batch(AActor* Owner, UHouseBlackHallSubsystem* Look, const TCHAR* Suffix, FHallMesh& Mesh,
    const FTransform& Transform, int32 Cull, bool WorldVertices = false)
{
    if (Mesh.NumTriangles() == 0) return 0;
    if (WorldVertices) for (auto& Pair : Mesh.Sections)
        for (FVector& Position : Pair.Value.V) Position -= Transform.GetLocation();
    const FString Name = FString(TEXT("ExteriorWoodsPassTrace")) + Suffix;
    TArray<FBox> NoCollision;
    auto* StaticMesh = HwBuildStaticMesh(Mesh, *FString(TEXT("SM_") + Name), Look, NoCollision);
    if (!StaticMesh) return 0;
    auto* Component = NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner, FName(*Name), RF_Transient);
    Owner->AddInstanceComponent(Component);
    Component->SetupAttachment(Owner->GetRootComponent());
    Component->SetMobility(EComponentMobility::Static);
    Component->SetStaticMesh(StaticMesh);
    Component->SetCastShadow(false);
    Component->bAffectDistanceFieldLighting = false;
    Component->bAffectDynamicIndirectLighting = false;
    Component->SetGenerateOverlapEvents(false);
    Component->SetCullDistances(FMath::RoundToInt(Cull * .80f), Cull);
    Component->RegisterComponent();
    Component->SetCollisionProfileName(TEXT("NoCollision"));
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->AddInstance(Transform, true);
    Component->BuildTreeIfOutdated(false, true);
    UE_LOG(LogTemp, Display, TEXT("WOODS_TRACE name=%s position=%s triangles=%d cull=%d collision=0 shadows=0 newLights=0 trailClearance=%.1f localGeometry=1"),
        *Name, *Transform.GetLocation().ToCompactString(), Mesh.NumTriangles(), Cull,
        HouseWoods::TrailDistance(Transform.GetLocation().X, Transform.GetLocation().Y));
    return 1;
}

TArray<FTree> Trees(AActor* Owner)
{
    TArray<FTree> Result;
    TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components(Owner);
    for (auto* Component : Components)
    {
        const FString Name = Component->GetName();
        const bool Oak = Name.StartsWith(TEXT("ExteriorTreeOak")), Maple = Name.StartsWith(TEXT("ExteriorTreeMaple"));
        if (!Oak && !Maple) continue;
        // Prefix length is derived rather than assuming UObject's unique suffix.
        const FString Prefix = Oak ? TEXT("ExteriorTreeOak") : TEXT("ExteriorTreeMaple");
        const int32 Variant = FCString::Atoi(*Name.Mid(Prefix.Len(), 1));
        for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
        {
            FTransform Transform; Component->GetInstanceTransform(Index, Transform, true);
            const FVector Location = Transform.GetLocation();
            if (Location.Y < 4900 || Location.Y > 18500 || FMath::Abs(Location.X) > 7900) continue;
            Result.Add({Component, Transform, Oak ? HouseNature::ETreeKind::Oak : HouseNature::ETreeKind::Maple, Variant, Index});
        }
    }
    return Result;
}

const FTree* Nearest(const TArray<FTree>& Candidates, FVector2D Desired, HouseNature::ETreeKind Kind)
{
    const FTree* Best = nullptr;
    float BestDistance = 250.f * 250.f;
    for (const FTree& Tree : Candidates)
    {
        if (Tree.Kind != Kind) continue;
        const FVector Position = Tree.Transform.GetLocation();
        const float Distance = FVector2D(Position.X - Desired.X, Position.Y - Desired.Y).SizeSquared();
        if (Distance < BestDistance) { Best = &Tree; BestDistance = Distance; }
    }
    return Best;
}

const FHallSection& BarkGeometry(const FTree& Tree, TMap<int32, FHallSection>& Cache)
{
    const int32 Key = int32(Tree.Kind) * 10 + Tree.Variant;
    if (!Cache.Contains(Key))
    {
        // Invoke the exact original deterministic local builder. Its private RNG
        // is independent of the scatter stream; no live tree is rebuilt/changed.
        FHallSection Actual;
        if (HouseCalderMapTrees::Bark(Tree.Component->GetStaticMesh(),Actual))
        { Cache.Add(Key,MoveTemp(Actual));return Cache.FindChecked(Key); }
        FHallMesh Model; TArray<FBox> IgnoredCollision;
        HouseNature::Tree(Model, Tree.Kind, Tree.Variant, IgnoredCollision);
        if (FHallSection* Bark = Model.Sections.Find(TEXT("EX_NatureBark"))) Cache.Add(Key, MoveTemp(*Bark));
        else Cache.Add(Key, FHallSection());
    }
    return Cache.FindChecked(Key);
}

bool BarkHit(const FTree& Tree, const FHallSection& Bark, FVector Start, FVector End, FVector& Position, FVector* Normal = nullptr)
{
    const FVector A = Tree.Transform.InverseTransformPosition(Start), Ray = Tree.Transform.InverseTransformPosition(End) - A;
    float Best = 1.00001f;
    FVector BestNormal;
    for (int32 Index = 0; Index + 2 < Bark.I.Num(); Index += 3)
    {
        const int32 IA = Bark.I[Index], IB = Bark.I[Index + 1], IC = Bark.I[Index + 2];
        const FVector Vertex = Bark.V[IA], Edge1 = Bark.V[IB] - Vertex, Edge2 = Bark.V[IC] - Vertex;
        const FVector P = FVector::CrossProduct(Ray, Edge2);
        const double Determinant = FVector::DotProduct(Edge1, P);
        if (FMath::Abs(Determinant) < 1.e-8) continue;
        const double Inverse = 1. / Determinant;
        const FVector Delta = A - Vertex;
        const double U = FVector::DotProduct(Delta, P) * Inverse;
        if (U < 0 || U > 1) continue;
        const FVector Q = FVector::CrossProduct(Delta, Edge1);
        const double V = FVector::DotProduct(Ray, Q) * Inverse;
        if (V < 0 || U + V > 1) continue;
        const double T = FVector::DotProduct(Edge2, Q) * Inverse;
        if (T < 0 || T > 1 || T >= Best) continue;
        Best = float(T);
        BestNormal = Bark.N[IA] * (1 - U - V) + Bark.N[IB] * U + Bark.N[IC] * V;
    }
    if (Best > 1.f) return false;
    Position = FMath::Lerp(Start, End, Best);
    if (Normal) *Normal = Tree.Transform.TransformVectorNoScale(BestNormal / Tree.Transform.GetScale3D()).GetSafeNormal();
    return true;
}

bool TrunkContact(const FTree& Tree, const FHallSection& Bark, float AboveGround, FVector Facing,
    FVector& Position, FVector& Normal)
{
    Facing.Z = 0; Facing.Normalize();
    const FVector Center = Tree.Transform.GetLocation() + FVector(0, 0, AboveGround);
    if (!BarkHit(Tree, Bark, Center + Facing * 160, Center - Facing * 160, Position, &Normal)) return false;
    // Sign boards remain upright; the nail reaches the exact measured surface.
    Normal = Facing;
    return true;
}

bool BranchSupport(const FTree& Tree, const FHallSection& Bark, float DesiredHeight, FSupport& Found, int32 ValidCandidate = 0)
{
    struct FCandidate { FVector Surface, Axis; float Score; };
    TArray<FCandidate> Candidates;
    const FVector Root = Tree.Transform.GetLocation();
    for (int32 Index = 0; Index + 2 < Bark.I.Num(); Index += 3)
    {
        const FVector A = Tree.Transform.TransformPosition(Bark.V[Bark.I[Index]]);
        const FVector B = Tree.Transform.TransformPosition(Bark.V[Bark.I[Index + 1]]);
        const FVector C = Tree.Transform.TransformPosition(Bark.V[Bark.I[Index + 2]]);
        const FVector Center = (A + B + C) / 3;
        const float Radius = FVector::Dist2D(Center, Root), Height = Center.Z - Root.Z;
        if (Radius < 95 || Radius > 245 || Height < 260 || Height > 590) continue;
        FVector Edges[] = {B - A, C - B, A - C};
        int32 Longest = Edges[1].SizeSquared() > Edges[0].SizeSquared() ? 1 : 0;
        if (Edges[2].SizeSquared() > Edges[Longest].SizeSquared()) Longest = 2;
        const FVector Axis = Edges[Longest].GetSafeNormal();
        if (FMath::Abs(Axis.Z) > .72f || Edges[Longest].SizeSquared() < 200) continue;
        const float TrailClearance = HouseWoods::TrailDistance(Center.X, Center.Y);
        if (TrailClearance < 450) continue;
        Candidates.Add({Center, Axis, FMath::Abs(Height - DesiredHeight) + FMath::Abs(Radius - 165) * .35f});
    }
    Candidates.Sort([](const FCandidate& Left, const FCandidate& Right) { return Left.Score < Right.Score; });
    int32 Valid=0;
    for (int32 Index = 0; Index < FMath::Min(80, Candidates.Num()); ++Index)
    {
        const FCandidate& Candidate = Candidates[Index];
        FVector Top, Bottom;
        const FVector Offset(0, 0, 27);
        if (!BarkHit(Tree, Bark, Candidate.Surface + Offset, Candidate.Surface - Offset, Top)
            || !BarkHit(Tree, Bark, Candidate.Surface - Offset, Candidate.Surface + Offset, Bottom)) continue;
        const float Diameter = (Top - Bottom).Size();
        if (Diameter < 6 || Diameter > 42) continue;
        if (Valid++ != ValidCandidate) continue;
        Found.Center = (Top + Bottom) * .5f;
        Found.Axis = Candidate.Axis;
        Found.Radius = Diameter * .5f;
        return true;
    }
    return false;
}

bool WrapRope(FHallMesh& Mesh, const FTree& Tree, const FHallSection& Bark, FVector Center, FVector Axis, FVector& Tail)
{
    // Every knot point is ray-fitted to the real branch surface. A guessed
    // circular collar is not sufficient around an irregular/leaning branch.
    const FVector Up = (FVector::UpVector - Axis * Axis.Z).GetSafeNormal();
    const FVector Side = FVector::CrossProduct(Axis, Up).GetSafeNormal();
    TArray<FVector> Points;
    constexpr int32 Steps = 32;
    for (int32 Step = 0; Step <= Steps; ++Step)
    {
        const float Angle = PI + Step * 2 * PI / Steps;
        const FVector Radial = Up * FMath::Cos(Angle) + Side * FMath::Sin(Angle);
        const FVector Along = Axis * (float(Step) / Steps * 1.8f - .9f);
        FVector Hit;
        if (!BarkHit(Tree, Bark, Center + Along + Radial * 40, Center + Along, Hit)) return false;
        Points.Add(Hit + Radial * .9f);
    }
    for (int32 Step = 0; Step + 1 < Points.Num(); ++Step) Beam(Mesh, Rope, Points[Step], Points[Step + 1], .62f, 6);
    Tail = Points.Last();
    return true;
}

void AttachmentLog(const TCHAR* Item, const FTree& Tree, FVector Position)
{
    UE_LOG(LogTemp, Display, TEXT("WOODS_TRACE_ATTACHMENT item=%s source=%s instance=%d species=%s variant=%d tree=%s support=%s exactBarkTriangleFit=1 originalTreeUnchanged=1"),
        Item, *Tree.Component->GetName(), Tree.Instance, HouseNature::TreeName(Tree.Kind), Tree.Variant,
        *Tree.Transform.GetLocation().ToCompactString(), *Position.ToCompactString());
}

int32 MakeSign(AActor* Owner, UHouseBlackHallSubsystem* Look, const FTree& Tree, const FHallSection& Bark, bool Paddle)
{
    FVector Contact, Normal;
    if (!TrunkContact(Tree, Bark, Paddle ? 148.f : 177.f, Paddle ? FVector(-1,-.35f,0) : FVector(-1,-.22f,0), Contact, Normal)) return 0;
    const FVector Across = FVector::CrossProduct(FVector::UpVector, Normal).GetSafeNormal();
    const FVector BarkAnchor=Contact;
    if (HouseCalderMapTrees::Enabled() && !Paddle)
    {
        // A flat board needs to clear the entire irregular trunk behind it,
        // not only its centre ray. Keep a real wooden spacer back to the bark.
        float Forward=0.f;
        for (const float X:{-39.f,-19.5f,0.f,19.5f,39.f})
            for (const float Z:{-23.f,-11.5f,0.f,11.5f,23.f})
            {
                const FVector Point=BarkAnchor+Across*X+FVector(0,0,Z);
                FVector Surface;
                if (BarkHit(Tree,Bark,Point+Normal*160.f,Point-Normal*160.f,Surface))
                    Forward=FMath::Max(Forward,float(FVector::DotProduct(Surface-BarkAnchor,Normal)));
            }
        Contact+=Normal*(Forward+1.f);
    }
    const FVector Center = Contact + Normal * (Paddle ? 1.1f : 1.6f);
    auto At = [&](float X, float Y, float Z) { return Center + Across * X + Normal * Y + FVector(0,0,Z); };
    FHallMesh Mesh;
    if (!Paddle && FVector::DistSquared(Contact,BarkAnchor)>1.f)
        Beam(Mesh,Wood,BarkAnchor,Contact,2.1f,6);
    if (!Paddle)
    {
        Mesh.Box(Wood, FTransform(FRotationMatrix::MakeFromYZ(Normal, FVector::UpVector).ToQuat(), Center), FVector(39,1.5f,23), UV);
        Mesh.QuadUV(Sign, At(-39,1.56f,-23), At(39,1.56f,-23), At(39,1.56f,23), At(-39,1.56f,23), Normal,
            {0,1},{1,1},{1,0},{0,0}, Across);
        for (float Height : {-16.f, 16.f})
        {
            FVector NailContact, UnusedNormal;
            if (TrunkContact(Tree, Bark, 177 + Height, Normal, NailContact, UnusedNormal))
                Beam(Mesh, Metal, NailContact - Normal, At(0,2.6f,Height), .45f, 7);
            Beam(Mesh, Rust, At(-1,2.9f,Height), At(1,2.9f,Height), .6f, 7);
        }
    }
    else
    {
        const TArray<FVector2D> Outline = {{-3.5f,39},{3.5f,39},{4,15},{11,7},{13,-22},{10,-34},{-10,-34},{-13,-22},{-11,7},{-4,15}};
        for (int32 Index = 0; Index < Outline.Num(); ++Index)
        {
            const FVector2D A = Outline[Index], B = Outline[(Index + 1) % Outline.Num()];
            const FVector FaceNormal = (Across * (A.Y - B.Y) + FVector::UpVector * (B.X - A.X)).GetSafeNormal();
            Mesh.Quad(Wood, At(A.X,-.9f,A.Y), At(B.X,-.9f,B.Y), At(B.X,.9f,B.Y), At(A.X,.9f,A.Y), FaceNormal, UV);
            for (float Face : {-1.f, 1.f})
                Mesh.Quad(Wood, At(0,Face*.9f,0), At(A.X,Face*.9f,A.Y), At(B.X,Face*.9f,B.Y), At(0,Face*.9f,0), Normal * Face, UV);
        }
        // An old plain paddle: no letters, crest, insignia or brand.
        Beam(Mesh, Dark, At(0,.92f,32), At(0,.96f,32), 1.45f, 9);
        FVector NailContact, UnusedNormal;
        if (TrunkContact(Tree, Bark, 180, Normal, NailContact, UnusedNormal)) Beam(Mesh, Metal, NailContact - Normal, At(0,2.2f,32), .58f, 8);
    }
    AttachmentLog(Paddle ? TEXT("plainPaddle") : TEXT("noTrespassing"), Tree, Contact);
    return Batch(Owner, Look, Paddle ? TEXT("PlainPaddle") : TEXT("NoTrespassing"), Mesh, FTransform(Center), 4500, true);
}

void Sleeve(FHallMesh& Mesh, FVector Shoulder, FVector Elbow, FVector Cuff, FVector Across, FVector Normal)
{
    const FVector Centers[] = {Shoulder, FMath::Lerp(Shoulder, Elbow, .5f), Elbow, FMath::Lerp(Elbow,Cuff,.5f), Cuff};
    const float Radii[] = {10,9.5f,8.3f,7.1f,6.f};
    for (int32 Ring = 0; Ring < 4; ++Ring) for (int32 Side = 0; Side < 10; ++Side)
    {
        auto At = [&](int32 Row, int32 Column)
        {
            const float Angle = Column * 2 * PI / 10;
            return Centers[Row] + Across * FMath::Cos(Angle) * Radii[Row] + Normal * FMath::Sin(Angle) * Radii[Row] * .32f;
        };
        const FVector A = At(Ring,Side), B = At(Ring,Side+1), C = At(Ring+1,Side+1), D = At(Ring+1,Side);
        Mesh.Quad(Cloth,A,B,C,D,FVector::CrossProduct(B-A,D-A).GetSafeNormal(),UV);
    }
    // Flattened open cuff, not a hand.
    Mesh.Quad(Dark, Cuff-Across*5.5f-Normal*1.6f, Cuff+Across*5.5f-Normal*1.6f,
        Cuff+Across*5.5f+Normal*1.6f, Cuff-Across*5.5f+Normal*1.6f, FVector(0,0,-1), UV);
}

int32 MakeCoat(AActor* Owner, UHouseBlackHallSubsystem* Look, const FTree& Tree, const FHallSection& Bark)
{
    FSupport Support;
    FHallMesh Mesh;
    FVector Tail;
    bool Attached=false;
    const int32 Attempts=HouseCalderMapTrees::Enabled()?80:1;
    for (int32 Candidate=0;Candidate<Attempts;++Candidate)
    {
        if (!BranchSupport(Tree,Bark,310,Support,Candidate)) break;
        FHallMesh CandidateMesh;
        if (!WrapRope(CandidateMesh,Tree,Bark,Support.Center,Support.Axis,Tail)) continue;
        Mesh=MoveTemp(CandidateMesh);Attached=true;break;
    }
    if (!Attached) return 0;
    const FVector Normal = FVector(-.88f,-.47f,0).GetSafeNormal();
    const FVector Across = FVector::CrossProduct(FVector::UpVector, Normal).GetSafeNormal();
    const FVector Collar = Tail - FVector(0,0,13);
    Beam(Mesh, Rope, Tail, Collar, .6f, 6);
    auto At = [&](float X, float Z, float Depth) { return Collar + Across*X + FVector(0,0,Z) + Normal*Depth; };
    // Narrow, flattened cloth with an open centre, sloping shoulders and empty
    // sleeves. No head, face, legs or body volume is placed inside the coat.
    constexpr int32 Rows = 9, Columns = 12;
    for (float Face : {-1.f, 1.f}) for (int32 Row = 0; Row < Rows; ++Row) for (int32 Column = 0; Column < Columns; ++Column)
    {
        if (Face > 0 && (Column == 5 || Column == 6)) continue;
        auto Point = [&](int32 R, int32 C)
        {
            const float V = R / float(Rows), U = C / float(Columns), Width = FMath::Lerp(18.f, 24.f, FMath::SmoothStep(0.f,.65f,V));
            const float X = (U-.5f)*2*Width;
            const float Top = -3.f - FMath::Abs(X)*.23f;
            const float Z = FMath::Lerp(Top,-81.f+FMath::Sin(U*11)*2.5f,V);
            const float Fold = FMath::Sin(U*31+V*.8f)*1.9f + FMath::Sin(U*15-V*2)*1.1f;
            return At(X,Z,Face*(1.9f+Fold*.25f)+Fold);
        };
        const FVector A=Point(Row,Column), B=Point(Row,Column+1), C=Point(Row+1,Column+1), D=Point(Row+1,Column);
        Mesh.Quad(Cloth,A,B,C,D,Normal*Face,UV);
    }
    Sleeve(Mesh, At(-18,-7,0), At(-29,-30,2), At(-25,-60,1), Across, Normal);
    Sleeve(Mesh, At(18,-7,0), At(29,-28,-1), At(31,-58,-1), Across, Normal);
    Beam(Mesh, Wood, At(-17,-7,0), At(0,-1,0), .55f, 5);
    Beam(Mesh, Wood, At(0,-1,0), At(17,-7,0), .55f, 5);
    for (int32 Button=0;Button<4;++Button) Beam(Mesh,Metal,At(5,-20-Button*14,3),At(5,-20-Button*14,3.5f),.72f,7);
    AttachmentLog(TEXT("emptyCoat"),Tree,Support.Center);
    return Batch(Owner,Look,TEXT("EmptyCoat"),Mesh,FTransform(Support.Center),6500,true);
}

int32 MakeSwing(AActor* Owner,UHouseBlackHallSubsystem* Look,const FTree& Tree,const FHallSection& Bark)
{
    FSupport Support;
    if(!BranchSupport(Tree,Bark,440,Support))return 0;
    FHallMesh Mesh;
    int32 Ropes=0;
    for(float Offset:{-17.f,17.f})
    {
        const FVector Probe=Support.Center+Support.Axis*Offset;
        FVector Top,Bottom;
        if(!BarkHit(Tree,Bark,Probe+FVector(0,0,27),Probe-FVector(0,0,27),Top)
            ||!BarkHit(Tree,Bark,Probe-FVector(0,0,27),Probe+FVector(0,0,27),Bottom))continue;
        FVector Tail;
        if(!WrapRope(Mesh,Tree,Bark,(Top+Bottom)*.5f,Support.Axis,Tail))continue;
        const FVector End=Ground(Tail.X+7,Tail.Y-4,Offset<0?78.f:103.f);
        FVector Previous=Tail;
        for(int32 Segment=1;Segment<=16;++Segment)
        {
            const float T=Segment/16.f;
            const FVector Current=FMath::Lerp(Tail,End,T)+FVector(FMath::Sin(T*PI)*3,FMath::Sin(T*2*PI)*1.8f,0);
            Beam(Mesh,Rope,Previous,Current,.70f,6);Previous=Current;
        }
        for(int32 Strand=0;Strand<5;++Strand)
        {
            const float Angle=Strand*2.39996f;
            Beam(Mesh,Rope,End,End+FVector(FMath::Cos(Angle)*3,FMath::Sin(Angle)*3,-7-Strand*1.8f),.15f,4);
        }
        ++Ropes;
    }
    if(Ropes!=2){UE_LOG(LogTemp,Warning,TEXT("WOODS_TRACE swing skipped: two supported ropes required"));return 0;}
    AttachmentLog(TEXT("seatlessSwing"),Tree,Support.Center);
    return Batch(Owner,Look,TEXT("SeatlessSwing"),Mesh,FTransform(Support.Center),6500,true);
}

int32 MakeFridge(AActor* Owner,UHouseBlackHallSubsystem* Look)
{
    FHallMesh Mesh;
    // Open-backed shell in upright local coordinates; the whole appliance is
    // tipped onto its side in the ravine. Exposed cavity, gasket and shelves
    // distinguish it from a generic rectangular box. No branding or decals.
    Mesh.BoxMinMax(Enamel,{-34,-29,0},{34,29,7},UV);
    Mesh.BoxMinMax(Enamel,{-34,-29,157},{34,29,164},UV);
    Mesh.BoxMinMax(Enamel,{-34,-29,7},{-28,29,157},UV);
    Mesh.BoxMinMax(Enamel,{28,-29,7},{34,29,157},UV);
    Mesh.BoxMinMax(Rust,{-28,23,7},{28,29,157},UV);
    Mesh.BoxMinMax(Dark,{-27,22.5f,8},{27,23,156},UV);
    for(float Level:{34.f,72.f,108.f})for(float X=-24;X<=24;X+=8)
        Beam(Mesh,Metal,{X,-23,Level},{X,22,Level},.45f,5);
    for(float Level:{34.f,72.f,108.f})for(float Y:{-22.f,21.f})Beam(Mesh,Metal,{-26,Y,Level},{26,Y,Level},.6f,5);
    const FVector Hinge(-34,-30,0);
    const FQuat DoorRotation(FVector::UpVector,FMath::DegreesToRadians(-34.f));
    auto Door=[&](FVector Point){return Hinge+DoorRotation.RotateVector(Point);};
    Mesh.Box(Enamel,FTransform(DoorRotation,Door({34,0,82})),{34,3,82},UV);
    for(float X:{4.f,64.f})Beam(Mesh,Dark,Door({X,3.4f,5}),Door({X,3.4f,159}),1.0f,6);
    for(float Z:{5.f,159.f})Beam(Mesh,Dark,Door({4,3.4f,Z}),Door({64,3.4f,Z}),1.0f,6);
    Beam(Mesh,Rust,Door({60,-5,84}),Door({60,-5,116}),1.6f,8);
    Beam(Mesh,Rust,Door({60,-2,84}),Door({60,-5,84}),1.6f,8);
    Beam(Mesh,Rust,Door({60,-2,116}),Door({60,-5,116}),1.6f,8);
    for(int32 Coil=0;Coil<8;++Coil)Beam(Mesh,Metal,{-22,31,20+Coil*15.f},{22,31,20+Coil*15.f},.7f,6);
    Ellipsoid(Mesh,Dark,{0,34,15},{15,8,10},10,5);
    const FQuat Rotation=FRotator(9,29,-86).Quaternion();
    const FVector2D XY(-4040,8090);
    float LowestGap=MAX_flt;
    // Fit the lowest part of the complete rotated shell against real terrain.
    for(const auto& Pair:Mesh.Sections)for(const FVector& Vertex:Pair.Value.V)
    {
        const FVector Rotated=Rotation.RotateVector(Vertex);
        LowestGap=FMath::Min(LowestGap,float(Rotated.Z-HouseWoods::Height(XY.X+Rotated.X,XY.Y+Rotated.Y)));
    }
    const FVector Location(XY.X,XY.Y,-LowestGap-3.f);
    return Batch(Owner,Look,TEXT("DumpedFridge"),Mesh,FTransform(Rotation,Location),7500);
}

int32 MakeStand(AActor* Owner,UHouseBlackHallSubsystem* Look,const FTree& Tree,const FHallSection& Bark)
{
    const FVector Root=Tree.Transform.GetLocation();
    FVector Contact,Normal;
    if(!TrunkContact(Tree,Bark,205,FVector(1,-.6f,0),Contact,Normal))return 0;
    const FVector Out=Normal,Across=FVector::CrossProduct(FVector::UpVector,Out).GetSafeNormal();
    const FVector Center=Ground(Root.X+Out.X*130,Root.Y+Out.Y*130,2);
    FHallMesh Mesh;
    Mesh.Box(Wood,FTransform(FRotationMatrix::MakeFromXZ(Across,FVector::UpVector).ToQuat(),
        Contact+Normal*3.5f),FVector(45,3,4),UV);
    for(float Side:{-1.f,1.f})
    {
        const FVector Top=Contact+Across*(Side*38)+Out*8;
        const FVector Foot=Ground(Top.X+Out.X*122,Top.Y+Out.Y*122,-2);
        Board(Mesh,Foot,Top,8,5);
        Board(Mesh,Foot+Across*(Side*9),Contact+Across*(-Side*39)+Out*10,5,3.5f);
    }
    const FVector PlatformNear=Contact+Out*12-FVector(0,0,9),PlatformFar=Center+Out*58+FVector(0,0,45);
    for(int32 Plank=0;Plank<7;++Plank)
    {
        if(Plank==3)continue;
        const FVector Offset=Across*((Plank-3)*12.f);
        Board(Mesh,PlatformNear+Offset,PlatformFar+Offset,10.5f,3);
    }
    // A split plank and the broken ladder remain grounded, not suspended.
    const FVector LadderA=Ground(Center.X+Across.X*75,Center.Y+Across.Y*75,4);
    const FVector LadderB=Ground(LadderA.X+Out.X*182,LadderA.Y+Out.Y*182,4);
    for(float Side:{-1.f,1.f})Board(Mesh,LadderA+Across*(Side*18),LadderB+Across*(Side*18),5,4);
    for(int32 Rung=0;Rung<7;++Rung)
    {
        if(Rung==4)continue;
        const FVector At=FMath::Lerp(LadderA,LadderB,(Rung+.4f)/7);
        Board(Mesh,At-Across*21,At+Across*21,4,3);
    }
    Beam(Mesh,Rust,Contact-Normal,Contact+Normal*14,1.1f,8);
    AttachmentLog(TEXT("collapsedStand"),Tree,Contact);
    return Batch(Owner,Look,TEXT("CollapsedStand"),Mesh,FTransform(Center),7500,true);
}

int32 MakeAshRing(AActor* Owner,UHouseBlackHallSubsystem* Look)
{
    const FVector Center=Ground(-2470,11680);
    FHallMesh Mesh;
    for(int32 Segment=0;Segment<28;++Segment)
    {
        const float A=Segment*2*PI/28,B=(Segment+1)*2*PI/28;
        const float RadiusA=58+5*FMath::Sin(Segment*2.7f),RadiusB=58+5*FMath::Sin((Segment+1)*2.7f);
        const FVector P=Ground(Center.X+FMath::Cos(A)*RadiusA,Center.Y+FMath::Sin(A)*RadiusA,.6f);
        const FVector Q=Ground(Center.X+FMath::Cos(B)*RadiusB,Center.Y+FMath::Sin(B)*RadiusB,.6f);
        Mesh.Quad(Ash,Center+FVector(0,0,.6f),P,Q,Center+FVector(0,0,.6f),FVector::UpVector,UV);
    }
    for(int32 Stone=0;Stone<13;++Stone)
    {
        const float Angle=Stone*2*PI/13;
        const FVector Position=Ground(Center.X+FMath::Cos(Angle)*72,Center.Y+FMath::Sin(Angle)*72,7);
        Ellipsoid(Mesh,TEXT("EX_WoodsRock0"),Position,FVector(11+Stone%3,10+Stone%4,9),8,4);
    }
    for(int32 Char=0;Char<9;++Char)
    {
        const float Angle=Char*2.39996f;
        const FVector A=Ground(Center.X+FMath::Cos(Angle)*24,Center.Y+FMath::Sin(Angle)*24,2);
        const FVector B=Ground(A.X+FMath::Cos(Angle+.8f)*22,A.Y+FMath::Sin(Angle+.8f)*22,2);
        Beam(Mesh,Dark,A,B,2.1f+Char%2,6);
    }
    return Batch(Owner,Look,TEXT("ColdAshRing"),Mesh,FTransform(Center),4000,true);
}

int32 WeatherHut(AActor* Owner,UHouseBlackHallSubsystem* Look)
{
    // Material override is scoped to the existing hut's own sections. Never
    // replace shared bark globally, rebuild its collision, move it or change
    // the doorway, steps, furniture, lantern or Coles Quarters lettering.
    FHallMesh Original;HouseWoods::Hut(Original);
    int32 Overrides=0;
    TInlineComponentArray<UProceduralMeshComponent*> Components(Owner);
    for(auto* Component:Components)
    {
        if(Component->GetName()!=TEXT("ExteriorWoodsHut"))continue;
        for(int32 Section=0;Section<Original.Order.Num();++Section)
        {
            const FName Key=Original.Order[Section];
            if(Key==TEXT("EX_WoodsHutWood")){Component->SetMaterial(Section,Look->Mat(Wood));++Overrides;}
            else if(Key==TEXT("EX_WoodsRoofMetal")){Component->SetMaterial(Section,Look->Mat(Rust));++Overrides;}
        }
    }
    FHallMesh Mesh;
    const FVector Center(HouseWoods::HutX,HouseWoods::HutY+295,HouseWoods::HutFloor);
    for(int32 Plank=0;Plank<3;++Plank)
    {
        const float X=HouseWoods::HutX-123+Plank*49;
        Board(Mesh,Ground(X+12,HouseWoods::HutY+335+Plank*16,1),FVector(X,HouseWoods::HutY+261,HouseWoods::HutFloor+92+Plank*18),11,2.4f);
    }
    UE_LOG(LogTemp,Display,TEXT("WOODS_TRACE_HUT materialOverrides=%d existingGeometryAndCollisionUnchanged=1 entranceAndSignUntouched=1"),Overrides);
    return Batch(Owner,Look,TEXT("HutWeathering"),Mesh,FTransform(Center),6000,true);
}
}

void HouseWoodsTraces::Build(AActor* Owner,UHouseBlackHallSubsystem* Look)
{
    if(!Owner||!Look)return;
    HouseWoodsTracesImpl::MakeLook(Look);
    const TArray<HouseWoodsTracesImpl::FTree> Trees=HouseWoodsTracesImpl::Trees(Owner);
    TMap<int32,FHallSection> BarkCache;
    int32 Batches=0,Attachments=0;
    Batches+=HouseWoodsTracesImpl::MakeFridge(Owner,Look);
    Batches+=HouseWoodsTracesImpl::MakeAshRing(Owner,Look);
    Batches+=HouseWoodsTracesImpl::WeatherHut(Owner,Look);
    auto Attach=[&](FVector2D Target,HouseNature::ETreeKind Kind,int32 Item)
    {
        const auto* Tree=HouseWoodsTracesImpl::Nearest(Trees,Target,Kind);
        if(!Tree){UE_LOG(LogTemp,Warning,TEXT("WOODS_TRACE missing real support item=%d target=(%.0f,%.0f); skipped"),Item,Target.X,Target.Y);return;}
        const FHallSection& Bark=HouseWoodsTracesImpl::BarkGeometry(*Tree,BarkCache);
        int32 Added=0;
        if(Item<2)Added=HouseWoodsTracesImpl::MakeSign(Owner,Look,*Tree,Bark,Item==1);
        else if(Item==2)Added=HouseWoodsTracesImpl::MakeCoat(Owner,Look,*Tree,Bark);
        else if(Item==3)Added=HouseWoodsTracesImpl::MakeSwing(Owner,Look,*Tree,Bark);
        else Added=HouseWoodsTracesImpl::MakeStand(Owner,Look,*Tree,Bark);
        if(!Added)UE_LOG(LogTemp,Warning,TEXT("WOODS_TRACE exact attachment unavailable item=%d; skipped rather than floating"),Item);
        Batches+=Added;Attachments+=Added;
    };
    Attach({-2339,5538},HouseNature::ETreeKind::Oak,0);
    Attach({168,12886},HouseNature::ETreeKind::Oak,1);
    Attach({-1766,8984},HouseNature::ETreeKind::Maple,2);
    Attach({-507,14624},HouseNature::ETreeKind::Oak,3);
    Attach({-5838,11302},HouseNature::ETreeKind::Oak,4);
    UE_LOG(LogTemp,Display,TEXT("WOODS_TRACES batches=%d attached=%d seed=%d provenance=originalProceduralPixelsAndGeometry existingTreesMoved=0 existingHutRebuilt=0 terrainChanged=0 collision=0 newLights=0"),
        Batches,Attachments,HouseWoodsTracesImpl::Seed);
}
