#include "HouseExteriorNature.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCalderTerrain.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseWestField.h"
#include "HouseWoodsDetail.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace HouseNature
{
namespace
{
const FName Bark = TEXT("EX_NatureBark"), BushLeaf = TEXT("EX_NatureShrub"), BushTip = TEXT("EX_NatureShrubTips");
const FName FrontTwig = TEXT("EX_FrontShrubTwig");
FName FrontLeaf(int Row,int Palette)
{
    return FName(*FString::Printf(TEXT("EX_FrontShrubLeaf%d_%d"),Row,Palette));
}
const FName Gold = TEXT("EX_NatureGold"), Copper = TEXT("EX_NatureCopper"), Rust = TEXT("EX_NatureRust"), GreenLeaf = TEXT("EX_NatureGreenLeaf");
const FName GreenGrass = TEXT("EX_NatureGrass"), DryGrass = TEXT("EX_NatureDryGrass"), Litter = TEXT("EX_NatureLitter");
const FName LitterGold = TEXT("EX_NatureLitterGold"), LitterRust = TEXT("EX_NatureLitterRust");
const FName BirchBark = TEXT("EX_NatureBirchBark");
const FName ConeCore=TEXT("EX_YardConeCore"),ConeScale=TEXT("EX_YardConeScale"),TwigCore=TEXT("EX_YardTwigCore");
const FName ScrapPaper=TEXT("EX_YardPaper"),CanMetal=TEXT("EX_YardCanMetal"),CanPaint=TEXT("EX_YardCanPaint"),CupPaint=TEXT("EX_YardCup");
FName SpeciesLeaf(ETreeKind Kind,int Palette,bool Single=false)
{
    return FName(*FString::Printf(TEXT("EX_Nature%s%s%d"),Kind==ETreeKind::Oak?TEXT("Oak"):TEXT("Birch"),Single?TEXT("Fallen"):TEXT("Leaf"),Palette));
}
constexpr float MacroX = -8200.f, MacroY = -10000.f, MacroW = 16400.f, MacroH = 18000.f;
float Rand(FRandomStream& R, float A, float B) { return R.FRandRange(A, B); }

float Turf(float X, float Y)
{
    // The same world-space field controls ground colour and actual blade density.
    return HallFbm(X * .0032f + 61.f, Y * .0032f - 19.f, 3, 437);
}
float Damp(float X, float Y) { return HallFbm(X * .00085f - 8.f, Y * .00085f + 35.f, 3, 443); }
float Grade(float Y) { return -60.f - 302.f * FMath::Clamp(Y / 1600.f, 0.f, 1.f); }
bool In(float X, float Y, float X0, float X1, float Y0, float Y1, float Margin = 0.f)
{ return X > X0 - Margin && X < X1 + Margin && Y > Y0 - Margin && Y < Y1 + Margin; }

// Follow the existing bank surface; do not alter the court or its collision.
float Bank(float X, float Y)
{
    const bool East = X > 0;
    const float T = FMath::Clamp((FMath::Abs(X) - 1530.f) / 1300.f, 0.f, 1.f);
    const float Edge = FMath::Clamp((Y - 1770) / 350.f, 0.f, 1.f) * FMath::Clamp((4300 - Y) / 450.f, 0.f, 1.f);
    const float Outer = 1.f - FMath::SmoothStep(2830.f, 3450.f, FMath::Abs(X));
    float Z = -362.f + (East ? 270.f : 200.f) * FMath::SmoothStep(0.f, 1.f, T) * Edge * Outer;
    if (!East)
    {
        const float Outside = FMath::Max(FMath::Abs(X + 2230) - 170.f, FMath::Abs(Y - 2660) - 95.f);
        const float Level = -362.f + 200.f * FMath::SmoothStep(0.f, 1.f, 700.f / 1300.f);
        Z = FMath::Lerp(Z, Level, 1.f - FMath::SmoothStep(0.f, 60.f, Outside));
    }
    return East?Z+.8f:HouseWestField::JoinBank(X,Y,Z+.8f);
}
bool OnBank(float X, float Y) { return FMath::Abs(X) > 1545 && FMath::Abs(X) < 3420 && Y > 1795 && Y < 4270; }
// The bank and the woods are frozen; the open ground (the lawns, the west field and on toward the campus) carries the
// Calder relief on top of its grade (the lattice surface the ground mesh is built from; exactly 0 in the frozen zones).
float Surface(float X, float Y) { return OnBank(X, Y) ? Bank(X, Y) : Y>=HouseWoods::YMin?HouseWoods::Height(X,Y):HouseWestField::RaisedHeight(X,Y,Grade(Y))+HouseCalderTerrain::ReliefLattice(X,Y); }

bool Lawn(float X, float Y)
{
    using namespace HouseExteriorSite;
    if (In(X,Y,-3400,-1420,-2200,1540,32) || In(X,Y,-1420,1360,-2190,1720,32) || In(X,Y,1360,3340,-2130,1540,32)) return false;
    if (Y > StreetY(-4350)-25 && Y < StreetY(-2950)+25) return false;
    if (In(X,Y,-32000,8700,StreetY(-2950),FrontSidewalkInnerY,25) || In(X,Y,-32000,8700,StreetY(-4600),StreetY(-4350),25)) return false;
    if (X > 8300 || In(X,Y,3450,4050,StreetY(-2990),1900,35) || In(X,Y,3450,6600,1900,4500,35)) return false;
    if (In(X,Y,6600,8720,2500,3100,35) || In(X,Y,-7200,-3700,-2500,-300,35) || In(X,Y,-5800,-5200,StreetY(-2990),-2500,35)) return false;
    if (In(X,Y,-9000,0,StreetY(-7400),StreetY(-4600),35) || In(X,Y,-5000,-4400,StreetY(-4600),StreetY(-4330),35)) return false;
    if (In(X,Y,-450,385,FrontSidewalkInnerY,-2190,28)) return false;
    if (In(X,Y,-3550,-1420,-2390,-2200,28) || In(X,Y,1360,3340,-2320,-2130,28)) return false;
    if (In(X,Y,-3550,-3400,-2200,1720,28) || In(X,Y,3340,3540,FrontSidewalkInnerY,1720,28)) return false;
    if (In(X,Y,-4490,-3400,-285,-105,25)) return false; // Parking access aisle to the house walk.
    if (In(X,Y,-7160,-5950,-2720,-2540,15) || In(X,Y,-5010,-3750,-2720,-2540,15)) return false;
    if (In(X,Y,-7150,-4515,-280,-110,15)) return false; // New mulched parking border.
    if (In(X,Y,-3550,3540,1540,1770,28) || In(X,Y,-1510,1510,1720,3910,28)) return false;
    if (In(X,Y,4170,4430,-2110,-1590,15) || In(X,Y,5870,6330,4120,4480,25)) return false;
    if (In(X,Y,-2500,-1960,2490,2850,28)) return false; // condenser pad
    if (In(X,Y,-1400,-497,-2510,-2236) || In(X,Y,423,1340,-2510,-2236)) return false; // mulched beds
    if (In(X,Y,-11800,-6200,700,4800) || In(X,Y,-9800,-200,StreetY(-11900),StreetY(-7300))) return false;
    return true;
}

UTexture2D* KeepTexture(UHouseBlackHallSubsystem* L, FName K, const FHallImage& I, bool bNormal = false, bool bWrap = false)
{
    UTexture2D* T = HallMakeTexture(I, !bNormal, bNormal, *K.ToString(), bWrap);
    if (T) L->Textures.Add(K,T);
    return T;
}

/** The hall painter intentionally discards alpha. Foliage needs its own alpha-preserving mip chain. */
UTexture2D* MaskTexture(UHouseBlackHallSubsystem* L, FName K, const FHallImage& I)
{
    UTexture2D* T = UTexture2D::CreateTransient(I.W,I.H,PF_B8G8R8A8,MakeUniqueObjectName(GetTransientPackage(),UTexture2D::StaticClass(),K));
    if (!T) return nullptr;
    T->SRGB = true; T->CompressionSettings = TC_Default;
    T->AddressX = T->AddressY = TA_Clamp; T->Filter = TF_Trilinear; T->LODGroup = TEXTUREGROUP_World;
    FTexturePlatformData* PD = T->GetPlatformData();
    if (!PD || PD->Mips.Num() == 0) return nullptr;
    TArray<FLinearColor> Level = I.Px;
    int W = I.W, H = I.H;
    float Coverage = 0.f;
    for (const FLinearColor& C : Level) Coverage += C.A >= .32f ? 1.f : 0.f;
    Coverage /= Level.Num();
    for (int Mip=0;;++Mip)
    {
        float Scale = 1.f;
        if (Mip > 0 && W*H > 16)
        {
            float Lo = .05f, Hi = 4.f;
            for (int Search=0;Search<8;++Search)
            {
                const float Mid = (Lo+Hi)*.5f;
                int Hits=0; for (const FLinearColor& C : Level) Hits += C.A*Mid >= .32f ? 1 : 0;
                if (Hits / float(Level.Num()) < Coverage) Lo=Mid; else Hi=Mid;
            }
            Scale=(Lo+Hi)*.5f;
        }
        TArray<FColor> Pixels; Pixels.SetNumUninitialized(W*H);
        for (int J=0;J<Level.Num();++J)
        {
            FLinearColor C = Level[J]; C.A = FMath::Clamp(C.A*Scale,0.f,1.f); Pixels[J]=C.ToFColor(true);
        }
        FTexture2DMipMap* Target = Mip==0 ? &PD->Mips[0] : new FTexture2DMipMap(W,H,1);
        if (Mip>0) PD->Mips.Add(Target);
        Target->SizeX=W; Target->SizeY=H;
        Target->BulkData.Lock(LOCK_READ_WRITE);
        void* Data=Target->BulkData.Realloc(int64(W)*H*4);
        FMemory::Memcpy(Data,Pixels.GetData(),int64(W)*H*4); Target->BulkData.Unlock();
        if (W==1 && H==1) break;
        const int NW=FMath::Max(1,W/2), NH=FMath::Max(1,H/2);
        TArray<FLinearColor> Next; Next.SetNumUninitialized(NW*NH);
        for(int Y=0;Y<NH;++Y)for(int X=0;X<NW;++X)
        {
            FLinearColor C(0,0,0,0); float Alpha=0;
            for(int V=0;V<2;++V)for(int U=0;U<2;++U)
            { const FLinearColor P=Level[FMath::Min(H-1,Y*2+V)*W+FMath::Min(W-1,X*2+U)]; C.R+=P.R*P.A;C.G+=P.G*P.A;C.B+=P.B*P.A;Alpha+=P.A; }
            if(Alpha>1e-5f){C.R/=Alpha;C.G/=Alpha;C.B/=Alpha;} C.A=Alpha*.25f;Next[Y*NW+X]=C;
        }
        Level=MoveTemp(Next);W=NW;H=NH;
    }
    T->UpdateResource();L->Textures.Add(K,T);return T;
}

FLinearColor LeafColour(int Kind)
{
    if(Kind==0)return FLinearColor(.55f,.305f,.055f);
    if(Kind==1)return FLinearColor(.48f,.14f,.035f);
    if(Kind==2)return FLinearColor(.27f,.065f,.025f);
    if(Kind==3)return FLinearColor(.13f,.21f,.045f);
    return FLinearColor(.065f,.145f,.038f);
}

void PaintLeaf(FHallImage& I,const FVector2D& Base,const FVector2D& Tip,float Width,const FLinearColor& C,int ShapeKind)
{
    const FVector2D Axis=Tip-Base, Side(-Axis.Y,Axis.X);
    TArray<FVector2D> Shape;
    auto P=[&](float U,float V){return Base+Axis*V+Side.GetSafeNormal()*Width*U;};
    if(ShapeKind==1)
    {
        const FVector2D Outline[]={{0,0},{-.16f,.2f},{-.62f,.23f},{-.39f,.40f},{-1,.45f},{-.66f,.63f},{-.78f,.82f},{-.25f,.69f},{0,1},{.25f,.69f},{.78f,.82f},{.66f,.63f},{1,.45f},{.39f,.4f},{.62f,.23f},{.16f,.2f}};
        for(const FVector2D& V:Outline)Shape.Add(P(V.X,V.Y));
    }
    else if(ShapeKind==2)
    {
        // Rounded oak lobes and a narrow petiole, rather than a maple outline.
        const FVector2D Outline[]={{0,0},{-.12f,.14f},{-.50f,.18f},{-.69f,.28f},{-.36f,.36f},{-.84f,.42f},{-1,.54f},{-.52f,.61f},{-.74f,.71f},{-.58f,.81f},{-.24f,.86f},{0,1},
            {.24f,.86f},{.58f,.81f},{.74f,.71f},{.52f,.61f},{1,.54f},{.84f,.42f},{.36f,.36f},{.69f,.28f},{.50f,.18f},{.12f,.14f}};
        for(const FVector2D& V:Outline)Shape.Add(P(V.X,V.Y));
    }
    else for(int K=0;K<28;++K)
    {
        const float A=K*2*PI/28;
        const float V=(1-FMath::Cos(A))*.5f;
        const float Serration=K%2?.91f:1.f;
        Shape.Add(P(FMath::Sin(A)*Serration,V));
    }
    I.FillPolygon(Shape,C);
    I.Stroke({Base,Tip},.9f,C*1.3f);
    for(int V=1;V<5;++V)for(float S:{-1.f,1.f})I.Stroke({P(0,V*.16f),P(S*.68f,V*.16f+.16f)},.65f,C*1.16f);
}

void PaintSpray(FHallImage& I,int Kind,bool Single,int ShapeKind=-1)
{
    constexpr int W=512;
    const FLinearColor C=LeafColour(Kind);
    I.Init(W,W,FLinearColor(C.R,C.G,C.B,0));
    const int Shape=ShapeKind<0?(Single||Kind<4?1:0):ShapeKind;
    if(Single) PaintLeaf(I,FVector2D(256,474),FVector2D(256,45),Shape==2?135:175,C,Shape);
    else
    {
        FRandomStream R(714+Kind*13);
        I.Stroke({FVector2D(260,496),FVector2D(242,320),FVector2D(275,120),FVector2D(260,40)},3.2f,FLinearColor(.13f,.083f,.036f));
        for(int J=0;J<7;++J)for(int S:{-1,1})
        {
            const FVector2D B(250+FMath::Sin(float(J))*14,440-J*53);
            const FVector2D End=B+FVector2D(S*Rand(R,67,108),Rand(R,-54,-30));
            I.Stroke({B,End},1.8f,FLinearColor(.19f,.12f,.038f));
            FLinearColor Tone=C*Rand(R,.68f,1.18f);Tone.A=1;
            PaintLeaf(I,End-FVector2D(S*18,-2),End+FVector2D(S*Rand(R,22,35),Rand(R,-32,-12)),Rand(R,25,42),Tone,Shape);
        }
    }
    for(int Y=0;Y<W;++Y)for(int X=0;X<W;++X)
    {
        FLinearColor& P=I.At(X,Y);const float A=P.A;
        const float N=HallNoise(X*.13f,Y*.13f,811+Kind),Fine=HallNoise(X*1.7f,Y*1.7f,815);
        P*=.82f+N*.26f+Fine*.05f;
        if(N>.79f)P*=.52f;P.A=A;
    }
}

void PaintGrass(FHallImage& I,bool Dry)
{
    constexpr int W=512;FRandomStream R(Dry?729:725);
    const FLinearColor C=Dry?FLinearColor(.31f,.255f,.12f):FLinearColor(.12f,.20f,.045f);
    I.Init(W,W,FLinearColor(C.R,C.G,C.B,0));
    for(int J=0;J<48;++J)
    {
        const float X=Rand(R,18,494),H=Rand(R,180,480),Lean=Rand(R,-25,25),Width=Rand(R,1.8f,3.8f);
        FLinearColor P=C*Rand(R,.6f,1.4f);P.A=1;
        I.FillPolygon({FVector2D(X-Width,508),FVector2D(X+Width,508),FVector2D(X+Lean*.32f+Width*.5f,508-H*.55f),FVector2D(X+Lean,508-H),FVector2D(X+Lean*.3f-Width*.3f,508-H*.55f)},P);
        I.Stroke({FVector2D(X,506),FVector2D(X+Lean*.3f,508-H*.55f),FVector2D(X+Lean,508-H)},.8f,P*1.2f);
    }
}

void Card(FHallMesh& M,FName K,const FVector& P,const FVector& U,const FVector& V)
{
    // Folded cards catch the light individually rather than making an opaque canopy shell.
    const FVector Face=FVector::CrossProduct(U,V).GetSafeNormal(),C=P+Face*U.Size()*.06f;
    // Blades and leaves share a softly rounded volume normal. Vertical cards should
    // not create alternating black faces under the existing overhead moonlight.
    const float UpBias=(K==GreenGrass||K==DryGrass)?3.3f:(K==Litter?0.f:.65f);
    const FVector N=(Face+FVector::UpVector*UpBias).GetSafeNormal();
    const FVector Tangent=(U-N*FVector::DotProduct(U,N)).GetSafeNormal();
    const FVector A=P-U-V,B=P+U-V,D=P-U+V,E=P+U+V;
    M.QuadUV(K,A,B,C+U,C-U,N,FVector2D(0,1),FVector2D(1,1),FVector2D(1,.5f),FVector2D(0,.5f),Tangent);
    M.QuadUV(K,C-U,C+U,E,D,N,FVector2D(0,.5f),FVector2D(1,.5f),FVector2D(1,0),FVector2D(0,0),Tangent);
}

void Tube(FHallMesh& M,const TArray<FVector>& Points,const TArray<float>& Radii,int Sides,FName BarkKey=Bark)
{
    if(Points.Num()<2||Points.Num()!=Radii.Num())return;
    FHallSection& S=M.Sec(BarkKey);
    const int Base=S.V.Num();float Along=0;
    const FVector First=(Points[1]-Points[0]).GetSafeNormal();
    const FVector Ref=FMath::Abs(First.Z)<.9f?FVector::UpVector:FVector::ForwardVector;
    for(int J=0;J<Points.Num();++J)
    {
        const FVector Axis=(Points[FMath::Min(J+1,Points.Num()-1)]-Points[FMath::Max(0,J-1)]).GetSafeNormal();
        const FVector U=FVector::CrossProduct(Axis,Ref).GetSafeNormal(),V=FVector::CrossProduct(Axis,U).GetSafeNormal();
        if(J>0)Along+=(Points[J]-Points[J-1]).Size()/115.f;
        const float Slope=J+1<Points.Num()?(Radii[J]-Radii[J+1])/FMath::Max(1.f,(Points[J+1]-Points[J]).Size()):0.f;
        for(int K=0;K<=Sides;++K)
        {
            const float A=K*2*PI/Sides;
            const FVector Radial=U*FMath::Cos(A)+V*FMath::Sin(A);
            // Slight ridges vary the silhouette as well as the bark normal map.
            const float Ridge=1+.028f*FMath::Sin(K*2.7f+J*.33f);
            S.V.Add(Points[J]+Radial*Radii[J]*Ridge);
            S.N.Add((Radial+Axis*Slope).GetSafeNormal());
            S.UV.Add(FVector2D(K/float(Sides)*FMath::Max(1.f,Radii[0]*.13f),-Along));
            S.T.Add(FProcMeshTangent((V*FMath::Cos(A)-U*FMath::Sin(A)).GetSafeNormal(),false));
        }
    }
    for(int J=0;J+1<Points.Num();++J)for(int K=0;K<Sides;++K)
    {
        const int A=Base+J*(Sides+1)+K,B=A+1,C=B+Sides+1,D=A+Sides+1;
        const bool Front=FVector::DotProduct(FVector::CrossProduct(S.V[B]-S.V[A],S.V[C]-S.V[A]),S.N[A])<0;
        if(Front)S.I.Append({A,B,C,A,C,D});else S.I.Append({A,C,B,A,D,C});
    }
}

void Branch(FHallMesh& M,const FVector& A,const FVector& Bend,const FVector& B,float R0,float R1,int Sides=8,FName BarkKey=Bark)
{
    TArray<FVector> P;TArray<float> Radius;
    // Reserve curved rings for the large limbs; fine twigs need only two sections.
    const float Length=(B-A).Size();const int Steps=Length>220?6:Length>65?3:2;
    for(int J=0;J<=Steps;++J)
    {
        const float T=J/float(Steps),Inv=1-T;
        P.Add(A*Inv*Inv+Bend*2*Inv*T+B*T*T);
        Radius.Add(FMath::Lerp(R0,R1,T));
    }
    Tube(M,P,Radius,Sides,BarkKey);
}

void LeafSprig(FHallMesh& M,FName K,ETreeKind Kind,const FVector& Anchor,const FVector& Direction,
    FRandomStream& R,float Length,int Count,TArray<FVector>* Origins)
{
    // Each small leaf has its own fold and petiole. The twig starts on a real
    // branch endpoint, so there are no floating, billboard-sized foliage sheets.
    const FVector Axis=Direction.GetSafeNormal();
    const FVector Ref=FMath::Abs(Axis.Z)<.9f?FVector::UpVector:FVector::ForwardVector;
    const FVector U0=FVector::CrossProduct(Axis,Ref).GetSafeNormal(),V0=FVector::CrossProduct(Axis,U0).GetSafeNormal();
    const FVector Bend=Anchor+Axis*Length*.5f+V0*Rand(R,-7,7);
    const FVector End=Anchor+Axis*Length+FVector(0,0,Kind==ETreeKind::Birch?-12.f:-4.f);
    const float StemRadius=Kind==ETreeKind::Birch?.16f:.28f;
    Tube(M,{Anchor,Bend,End},{StemRadius,StemRadius*.55f,.035f},5);
    const float Phase=Rand(R,0,2*PI);
    for(int J=0;J<Count;++J)
    {
        const float T=.12f+(J+.5f)/Count*.84f,Inv=1-T;
        const FVector Base=Anchor*Inv*Inv+Bend*2*Inv*T+End*T*T;
        const FVector Tangent=((Bend-Anchor)*Inv+(End-Bend)*T).GetSafeNormal();
        const float Angle=Phase+J*2.39996f+Rand(R,-.2f,.2f);
        const FVector Side=U0*FMath::Cos(Angle)+V0*FMath::Sin(Angle);
        const FVector Along=(Side*.9f+Tangent*.42f+FVector(0,0,-.12f)).GetSafeNormal();
        const float Size=Rand(R,.78f,1.18f),HalfLength=(Kind==ETreeKind::Birch?5.8f:10.f)*Size;
        const float HalfWidth=(Kind==ETreeKind::Maple?7.5f:Kind==ETreeKind::Oak?5.8f:4.1f)*Size;
        const FVector LeafBase=Base+Along*Rand(R,1.5f,3.8f);
        FVector Across=FVector::CrossProduct(Along,FVector::UpVector).GetSafeNormal();
        if(Across.IsNearlyZero())Across=U0;
        Across=Across.RotateAngleAxis(Rand(R,-65,65),Along);
        const FVector U=Across*HalfWidth,V=Along*HalfLength,P=LeafBase+V;
        const FVector Face=FVector::CrossProduct(U,V).GetSafeNormal(),C=P+Face*Rand(R,.45f,1.1f);
        const FVector N=(Face*.55f+FVector::UpVector*.7f).GetSafeNormal();
        const FVector Tan=(U-N*FVector::DotProduct(U,N)).GetSafeNormal();
        const FVector A=P-U-V,B=P+U-V,D=P-U+V,E=P+U+V;
        M.QuadUV(K,A,B,C+U,C-U,N,{0,1},{1,1},{1,.5f},{0,.5f},Tan);
        M.QuadUV(K,C-U,C+U,E,D,N,{0,.5f},{1,.5f},{1,0},{0,0},Tan);
        const FVector PetioleAcross=Across*.075f;
        M.Quad(Bark,Base-PetioleAcross,Base+PetioleAcross,LeafBase+PetioleAcross,LeafBase-PetioleAcross,Face,FHallUV::World(7));
        if(Origins)Origins->Add(P);
    }
}

void CuppedShrubLeaf(FHallMesh& M,FName Key,const FVector& Base,const FVector& Along,
    const FVector& Across,float Length,float Width,float Cup)
{
    // A centre rib and four curved length segments make each leaf a little volume,
    // not a large flat sheet. The alpha silhouette supplies the oval edge/veins.
    FVector Face=FVector::CrossProduct(Across,Along).GetSafeNormal();
    if(Face.Z<0)Face=-Face;
    auto Point=[&](float T,float Side)
    {
        const float Fold=Cup*FMath::Sin(T*PI)*(1-FMath::Abs(Side)*.85f);
        const float Curl=Cup*.6f*T*T;
        return Base+Along*(T*Length)+Across*(Side*Width)+Face*(Fold-Curl);
    };
    for(int Row=0;Row<4;++Row)for(int Side=0;Side<2;++Side)
    {
        const float A=Row*.25f,B=(Row+1)*.25f,U0=Side-1.f,U1=float(Side);
        const FVector P0=Point(A,U0),P1=Point(A,U1),P2=Point(B,U1),P3=Point(B,U0);
        FVector N=FVector::CrossProduct(P1-P0,P2-P0).GetSafeNormal();
        if(FVector::DotProduct(N,Face)<0)N=-N;
        N=(N*.72f+FVector::UpVector*.45f).GetSafeNormal();
        const FVector Tangent=(Across-N*FVector::DotProduct(Across,N)).GetSafeNormal();
        M.QuadUV(Key,P0,P1,P2,P3,N,{(U0+1)*.5f,1-A},{(U1+1)*.5f,1-A},
            {(U1+1)*.5f,1-B},{(U0+1)*.5f,1-B},Tangent);
    }
}

void FrontSprig(FHallMesh& M,const FVector& Anchor,const FVector& End,FRandomStream& R,int Palette)
{
    const FVector Axis=(End-Anchor).GetSafeNormal();
    const FVector Ref=FMath::Abs(Axis.Z)<.9f?FVector::UpVector:FVector::ForwardVector;
    const FVector U=FVector::CrossProduct(Axis,Ref).GetSafeNormal(),V=FVector::CrossProduct(Axis,U).GetSafeNormal();
    const FVector Bend=FMath::Lerp(Anchor,End,.55f)+V*Rand(R,-3,3);
    Tube(M,{Anchor,Bend,End},{.23f,.12f,.025f},5,FrontTwig);
    const float Phase=Rand(R,0,2*PI);
    for(int Pair=0;Pair<6;++Pair)for(int Side=0;Side<2;++Side)
    {
        const float T=.17f+Pair*.145f,Inv=1-T;
        const FVector Stem=Anchor*Inv*Inv+Bend*2*Inv*T+End*T*T;
        const FVector Tangent=((Bend-Anchor)*Inv+(End-Bend)*T).GetSafeNormal();
        const float Angle=Phase+Pair*.85f+Side*PI;
        const FVector Out=U*FMath::Cos(Angle)+V*FMath::Sin(Angle);
        const FVector Along=(Out*.9f+Tangent*.45f+FVector(0,0,Rand(R,-.1f,.25f))).GetSafeNormal();
        FVector Across=FVector::CrossProduct(Along,FVector::UpVector).GetSafeNormal();
        if(Across.IsNearlyZero())Across=U;
        Across=Across.RotateAngleAxis(Rand(R,-40,40),Along);
        const FVector LeafBase=Stem+Along*Rand(R,.55f,1.1f);
        M.Quad(FrontTwig,Stem-Across*.045f,Stem+Across*.045f,LeafBase+Across*.045f,
            LeafBase-Across*.045f,FVector::CrossProduct(Across,Along).GetSafeNormal(),FHallUV::World(5));
        const float Size=Rand(R,.85f,1.18f)*(Pair==5?.78f:1.f);
        CuppedShrubLeaf(M,FrontLeaf(0,Pair==5&&Palette!=1?2:Palette),LeafBase,Along,Across,
            7.4f*Size,2.45f*Size,Rand(R,.3f,.7f));
    }
}

UMaterialInstanceDynamic* NatureTextured(UHouseBlackHallSubsystem* L,FName Key,UTexture2D* Base,UTexture2D* Normal,
    float Rough,const FLinearColor& Tint=FLinearColor::White,float NormalStrength=1.f,float Metal=0.f)
{
    UMaterialInterface* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured.M_NaturePropTextured"));
    if(!Parent)return HallMakeTextured(L,Key,Base,Normal,Rough,Tint,NormalStrength,Metal);
    UMaterialInstanceDynamic* Mat=UMaterialInstanceDynamic::Create(Parent,L);
    if(Base)Mat->SetTextureParameterValue(TEXT("BaseMap"),Base);
    if(Normal)Mat->SetTextureParameterValue(TEXT("NormalMap"),Normal);
    Mat->SetScalarParameterValue(TEXT("UVScale"),1.f);
    Mat->SetScalarParameterValue(TEXT("Roughness"),Rough);
    Mat->SetScalarParameterValue(TEXT("Metallic"),Metal);
    Mat->SetScalarParameterValue(TEXT("NormalStrength"),NormalStrength);
    Mat->SetVectorParameterValue(TEXT("Tint"),Tint);
    Mat->SetVectorParameterValue(TEXT("Emission"),FLinearColor::Black);
    L->Materials.Add(Key,Mat);return Mat;
}

void NaturePlain(UHouseBlackHallSubsystem* L,FName Key,const FLinearColor& Tint,float Rough,float Metal=0.f)
{
    UMaterialInterface* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropPlain.M_NaturePropPlain"));
    if(!Parent){HallMakePlain(L,Key,Tint,Rough,Metal);return;}
    UMaterialInstanceDynamic* Mat=UMaterialInstanceDynamic::Create(Parent,L);
    Mat->SetVectorParameterValue(TEXT("Tint"),Tint);Mat->SetVectorParameterValue(TEXT("Emission"),FLinearColor::Black);
    Mat->SetScalarParameterValue(TEXT("Roughness"),Rough);Mat->SetScalarParameterValue(TEXT("Metallic"),Metal);
    L->Materials.Add(Key,Mat);
}

FName FallKey(FRandomStream& R,int Variant)
{
    const float T=R.FRand();
    if(T<.1f)return GreenLeaf;
    if(Variant==0)return T<.78f?Gold:Copper;
    if(Variant==1)return T<.7f?Copper:Rust;
    return T<.58f?Gold:T<.85f?Rust:Copper;
}
}

void CreateLook(UHouseBlackHallSubsystem* L)
{
    if(!L||L->Materials.Contains(Bark))return;
    UMaterialInterface* GroundParent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NatureGround.M_NatureGround"));
    UMaterialInterface* PlantParent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NatureFoliage.M_NatureFoliage"));
    if(!GroundParent||!PlantParent){UE_LOG(LogTemp,Error,TEXT("AUTUMN_NATURE missing saved foliage masters"));return;}
    FHallImage I,N;TArray<float> Height;
    constexpr int Res=512;
    I.Init(Res,Res,FLinearColor::Black);Height.Init(0,Res*Res);
    for(int Y=0;Y<Res;++Y)for(int X=0;X<Res;++X)
    {
        const float Warp=HallNoise(X*.055f,Y*.012f,821)*14.f;
        const float Grain=HallFbm((X+Warp)*.11f,Y*.009f,4,823);
        const float Furrow=FMath::Pow(FMath::Max(0.f,FMath::Sin((X+Warp)*.34f+HallNoise(X*.02f,Y*.05f,827)*9.f)),6.f);
        const float V=.20f+Grain*.16f-Furrow*.16f+HallNoise(X*1.6f,Y*.5f,829)*.035f;
        I.At(X,Y)=FLinearColor(V*.76f,V*.62f,V*.47f);Height[Y*Res+X]=Grain*.35f-Furrow*.5f;
    }
    UTexture2D* B=KeepTexture(L,TEXT("T_EX_NatureBark"),I,false,true);
    HallHeightToNormal(Height,Res,Res,3.1f,true,N);
    UTexture2D* BN=KeepTexture(L,TEXT("T_EX_NatureBarkN"),N,true,true);
    auto Photo=[&](const TCHAR* Name){return LoadObject<UTexture2D>(nullptr,*FString::Printf(TEXT("/Game/SeniorSendOff/House/ExteriorNature/%s.%s"),Name,Name));};
    if(UTexture2D* T=Photo(TEXT("T_Nature_Bark_Base")))B=T;
    if(UTexture2D* T=Photo(TEXT("T_Nature_Bark_Normal")))BN=T;
    NatureTextured(L,Bark,B,BN,.98f,FLinearColor(.85f,.77f,.66f),1.1f);
    I.Init(Res,Res,FLinearColor(.65f,.63f,.56f));
    for(int Y=0;Y<Res;++Y)for(int X=0;X<Res;++X)
    {
        const float Grain=HallNoise(X*.07f,Y*.12f,2201);
        const float Scar=HallNoise(X*.022f,Y*.17f,2203);
        const float Dark=Scar>.78f?FMath::SmoothStep(.78f,.91f,Scar):0.f;
        const float V=.62f+Grain*.17f-Dark*.63f;
        I.At(X,Y)=FLinearColor(V,V*.98f,V*.90f);Height[Y*Res+X]=Grain*.12f-Dark*.2f;
    }
    HallHeightToNormal(Height,Res,Res,2.f,true,N);
    NatureTextured(L,BirchBark,KeepTexture(L,TEXT("T_EX_BirchBark"),I,false,true),KeepTexture(L,TEXT("T_EX_BirchBarkN"),N,true,true),.91f,FLinearColor::White,.6f);

    // Small-scale thatch and grit, with a separate non-repeating colour field over the whole yard.
    I.Init(Res,Res,FLinearColor(.44f,.44f,.44f));FRandomStream R(839);
    for(int Y=0;Y<Res;++Y)for(int X=0;X<Res;++X)
    {const float V=.26f+HallFbm(X*.09f,Y*.09f,3,841)*.35f+HallNoise(X*1.7f,Y*1.7f,843)*.12f;I.At(X,Y)=FLinearColor(V,V,V);}
    for(int J=0;J<3300;++J)
    {
        const FVector2D A(Rand(R,0,Res),Rand(R,0,Res)),D(Rand(R,-7,7),Rand(R,2,25));const float V=Rand(R,.21f,.8f);
        for(int U=-1;U<=1;++U)for(int Vv=-1;Vv<=1;++Vv)I.Stroke({A+FVector2D(U*Res,Vv*Res),A+D+FVector2D(U*Res,Vv*Res)},Rand(R,.6f,1.4f),FLinearColor(V,V,V));
    }
    UTexture2D* Detail=KeepTexture(L,TEXT("T_EX_NatureTurf"),I,false,true);
    for(int J=0;J<Height.Num();++J)Height[J]=I.Px[J].R*.42f;
    HallHeightToNormal(Height,Res,Res,2.2f,true,N);
    UTexture2D* Normal=KeepTexture(L,TEXT("T_EX_NatureTurfN"),N,true,true);
    constexpr int MacroRes=1024;I.Init(MacroRes,MacroRes,FLinearColor::Black);
    for(int Y=0;Y<MacroRes;++Y)for(int X=0;X<MacroRes;++X)
    {
        const float WX=MacroX+X*MacroW/MacroRes,WY=MacroY+Y*MacroH/MacroRes;
        const float Patch=Turf(WX,WY),Moist=Damp(WX,WY);
        const float Healthy=FMath::SmoothStep(.47f,.67f,Patch),Bare=1-FMath::SmoothStep(.32f,.44f,Patch);
        I.At(X,Y)=FLinearColor(Healthy,Bare,Moist,1);
    }
    UMaterialInstanceDynamic* Ground=UMaterialInstanceDynamic::Create(GroundParent,L);
    Ground->SetTextureParameterValue(TEXT("BaseMap"),Detail);Ground->SetTextureParameterValue(TEXT("NormalMap"),Normal);
    if(UTexture2D* T=Photo(TEXT("T_Nature_GrassGreen_Base")))Ground->SetTextureParameterValue(TEXT("BaseMap"),T);
    if(UTexture2D* T=Photo(TEXT("T_Nature_GrassGreen_Normal")))Ground->SetTextureParameterValue(TEXT("NormalMap"),T);
    if(UTexture2D* T=Photo(TEXT("T_Nature_GrassDry_Base")))Ground->SetTextureParameterValue(TEXT("DryMap"),T);
    if(UTexture2D* T=Photo(TEXT("T_Nature_GrassDry_Normal")))Ground->SetTextureParameterValue(TEXT("DryNormalMap"),T);
    Ground->SetTextureParameterValue(TEXT("MacroMap"),KeepTexture(L,TEXT("T_EX_NatureMacro"),I));
    L->Materials.Add(TEXT("EX_Grass"),Ground);

    auto Plant=[&](FName Key,FHallImage& Paint,float Wind,float Transmission)
    {
        UTexture2D* T=MaskTexture(L,*FString::Printf(TEXT("T_%s"),*Key.ToString()),Paint);
        TArray<float> LeafHeight;LeafHeight.SetNum(Paint.W*Paint.H);
        for(int J=0;J<LeafHeight.Num();++J)LeafHeight[J]=Paint.Px[J].A*Paint.Px[J].GetLuminance()*.3f;
        HallHeightToNormal(LeafHeight,Paint.W,Paint.H,.6f,false,N);
        UMaterialInstanceDynamic* M=UMaterialInstanceDynamic::Create(PlantParent,L);
        M->SetTextureParameterValue(TEXT("BaseMap"),T);
        M->SetTextureParameterValue(TEXT("NormalMap"),KeepTexture(L,*FString::Printf(TEXT("T_%sN"),*Key.ToString()),N,true));
        M->SetScalarParameterValue(TEXT("WindStrength"),Wind);M->SetScalarParameterValue(TEXT("Transmission"),Transmission);L->Materials.Add(Key,M);
    };
    UMaterialInterface* TreeParent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NatureTreeFoliage.M_NatureTreeFoliage"));
    UTexture2D* Singles=Photo(TEXT("T_Nature_LeafSingles"));
    const bool PhotoReady=TreeParent&&Singles;
    auto AtlasPlant=[&](FName Key,ETreeKind Kind,int Palette,float Wind)
    {
        UMaterialInstanceDynamic* Mat=UMaterialInstanceDynamic::Create(TreeParent,L);
        Mat->SetTextureParameterValue(TEXT("BaseMap"),Singles);
        // A half-texel inset keeps adjacent atlas cells out of the filtered edge.
        const float Inset=.0005f,Step=1.f/3.f;
        Mat->SetVectorParameterValue(TEXT("AtlasRect"),FLinearColor(Palette*Step+Inset,int(Kind)*Step+Inset,Step-2*Inset,Step-2*Inset));
        Mat->SetScalarParameterValue(TEXT("WindStrength"),Wind);
        Mat->SetScalarParameterValue(TEXT("Transmission"),Wind>0?.42f:.18f);
        L->Materials.Add(Key,Mat);
    };
    const FName Keys[]={Gold,Copper,Rust,GreenLeaf};
    for(int K=0;K<4;++K)
    {
        if(PhotoReady)AtlasPlant(Keys[K],ETreeKind::Maple,K%3,1.8f);
        else {PaintSpray(I,K,true);Plant(Keys[K],I,2.4f,.7f);}
    }
    if(PhotoReady)Cast<UMaterialInstanceDynamic>(L->Mat(GreenLeaf))->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.75f,.98f,.75f));
    PaintSpray(I,4,false);Plant(BushLeaf,I,.6f,.7f);
    PaintSpray(I,4,false);Plant(BushTip,I,.9f,.7f);
    if(UMaterialInstanceDynamic* Tips=Cast<UMaterialInstanceDynamic>(L->Mat(BushTip)))Tips->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1.25f,1.12f,.73f));
    // Do not replace the older bush master: parking, neighbours and woods keep it.
    NatureTextured(L,FrontTwig,B,BN,.92f,FLinearColor(.72f,.57f,.40f),.85f);
    UMaterialInterface* FrontParent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/FrontBushDetail/M_FrontShrubFoliage.M_FrontShrubFoliage"));
    UTexture2D* FrontAtlas=LoadObject<UTexture2D>(nullptr,TEXT("/Game/SeniorSendOff/House/FrontBushDetail/T_FrontShrubAtlas.T_FrontShrubAtlas"));
    for(int Row=0;Row<3;++Row)for(int Palette=0;Palette<3;++Palette)
    {
        UMaterialInstanceDynamic* Mat=nullptr;
        if(FrontParent&&FrontAtlas)
        {
            Mat=UMaterialInstanceDynamic::Create(FrontParent,L);
            Mat->SetTextureParameterValue(TEXT("BaseMap"),FrontAtlas);
            const float Step=1.f/3.f,Inset=.001f;
            Mat->SetVectorParameterValue(TEXT("AtlasRect"),FLinearColor(Palette*Step+Inset,Row*Step+Inset,Step-2*Inset,Step-2*Inset));
            // New-growth leaves stay green, with just a few paler tips.
            Mat->SetVectorParameterValue(TEXT("Tint"),Palette==2?FLinearColor(.78f,.9f,.86f):FLinearColor(.92f,.97f,.94f));
            Mat->SetScalarParameterValue(TEXT("WindStrength"),Row==0?.55f:.8f);
        }
        else
        {
            Mat=Cast<UMaterialInstanceDynamic>(L->Mat(BushLeaf));
            UE_LOG(LogTemp,Error,TEXT("FRONT_BUSH missing dedicated atlas or material"));
        }
        L->Materials.Add(FrontLeaf(Row,Palette),Mat);
    }
    const FName Fallen[]={LitterGold,Litter,LitterRust};
    for(int Palette=0;Palette<3;++Palette)
    {
        if(PhotoReady)AtlasPlant(Fallen[Palette],ETreeKind::Maple,Palette,0.f);
        else {PaintSpray(I,Palette,true);Plant(Fallen[Palette],I,0.f,.08f);}
    }
    for(ETreeKind Kind:{ETreeKind::Oak,ETreeKind::Birch})for(int Palette=0;Palette<3;++Palette)
    {
        if(PhotoReady)
        {
            AtlasPlant(SpeciesLeaf(Kind,Palette),Kind,Palette,Kind==ETreeKind::Oak?1.5f:2.6f);
            AtlasPlant(SpeciesLeaf(Kind,Palette,true),Kind,Palette,0.f);
        }
        else
        {
            PaintSpray(I,Palette,true,Kind==ETreeKind::Oak?2:0);Plant(SpeciesLeaf(Kind,Palette),I,Kind==ETreeKind::Oak?2.f:3.1f,.72f);
            PaintSpray(I,Palette,true,Kind==ETreeKind::Oak?2:0);Plant(SpeciesLeaf(Kind,Palette,true),I,0.f,.12f);
        }
    }
    // Upright grass uses actual tapered blade geometry, so it needs no opaque tuft cards.
    UMaterialInterface* BladeParent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NatureGrassBlades.M_NatureGrassBlades"));
    if(BladeParent)
    {
        for(int32 K=0;K<2;++K)
        {
            UMaterialInstanceDynamic* M=UMaterialInstanceDynamic::Create(BladeParent,L);
            M->SetVectorParameterValue(TEXT("Tint"),K==0?FLinearColor(.115f,.20f,.035f):FLinearColor(.32f,.25f,.095f));
            M->SetScalarParameterValue(TEXT("WindStrength"),K==0?2.7f:3.8f);
            L->Materials.Add(K==0?GreenGrass:DryGrass,M);
        }
    }
    else UE_LOG(LogTemp,Error,TEXT("AUTUMN_NATURE missing upright grass material"));
    // Tiny yard props share rough, weathered materials instead of adding lights or actors.
    I.Init(128,128,FLinearColor::Black);Height.SetNum(128*128);
    for(int Y=0;Y<128;++Y)for(int X=0;X<128;++X)
    {
        const float Grain=HallFbm(X*.15f,Y*.07f,3,3121),Crack=FMath::Pow(FMath::Abs(FMath::Sin(X*.27f+HallNoise(X*.06f,Y*.09f,3123)*7)),14.f);
        const float V=.22f+Grain*.19f-Crack*.14f;
        I.At(X,Y)=FLinearColor(V,V*.62f,V*.33f);Height[Y*128+X]=Grain*.2f-Crack*.25f;
    }
    HallHeightToNormal(Height,128,128,1.4f,true,N);
    UTexture2D* ConeBase=KeepTexture(L,TEXT("T_EX_YardCone"),I,false,true),*ConeN=KeepTexture(L,TEXT("T_EX_YardConeN"),N,true,true);
    NatureTextured(L,ConeScale,ConeBase,ConeN,.96f,FLinearColor::White,.8f);
    NatureTextured(L,ConeCore,ConeBase,ConeN,.98f,FLinearColor(.46f,.39f,.30f),.5f);
    NaturePlain(L,TwigCore,FLinearColor(.29f,.20f,.105f),.98f);
    for(int Y=0;Y<128;++Y)for(int X=0;X<128;++X)
    {
        const float Stain=HallFbm(X*.026f,Y*.03f,3,3141);
        I.At(X,Y)=FLinearColor(.58f,.56f,.49f)*(.56f+Stain*.6f);
    }
    NatureTextured(L,ScrapPaper,KeepTexture(L,TEXT("T_EX_YardPaper"),I),nullptr,.99f,FLinearColor::White,0.f);
    NaturePlain(L,CanMetal,FLinearColor(.42f,.43f,.41f),.56f,.75f);
    for(int Y=0;Y<128;++Y)for(int X=0;X<128;++X)
    {
        const float Wear=.72f+HallFbm(X*.08f,Y*.08f,3,3151)*.28f;
        const FLinearColor C=Y>29&&Y<96?FLinearColor(.24f,.045f,.026f):FLinearColor(.61f,.57f,.43f);
        I.At(X,Y)=C*Wear;
    }
    NatureTextured(L,CanPaint,KeepTexture(L,TEXT("T_EX_YardCan"),I,false,true),nullptr,.65f,FLinearColor::White,0.f,.2f);
    NaturePlain(L,CupPaint,FLinearColor(.31f,.035f,.023f),.87f);
    UE_LOG(LogTemp,Display,TEXT("AUTUMN_NATURE detailed tree atlases=%d; wind, grass and sparse yard materials ready"),PhotoReady);
}

void MapleTree(FHallMesh& M,int Variant,TArray<FBox>& Col,TArray<FVector>* Origins)
{
    FRandomStream R(901+Variant*131);
    const float S=1.f,Rt=30.f;
    auto Trunk=[&](float Z){return FVector(FMath::Sin(Z/166+Variant)*22*S,FMath::Cos(Z/138+Variant)*17*S,Z*S);};
    TArray<FVector> TrunkPoints;TArray<float> TrunkRadii;
    TrunkPoints.Add(FVector(0,0,-16));TrunkRadii.Add(Rt*1.38f);
    for(int K=0;K<12;++K)
    {
        TrunkPoints.Add(Trunk((K+1)*55.3f));
        TrunkRadii.Add(FMath::Lerp(Rt,3.5f,FMath::Pow((K+1)/12.f,.8f)));
    }
    Tube(M,TrunkPoints,TrunkRadii,20);
    for(int J=0;J<7;++J)
    {
        const float A=J*2*PI/7+Rand(R,-.2f,.2f);const FVector D(FMath::Cos(A),FMath::Sin(A),0);
        Branch(M,FVector(0,0,25*S)+D*12*S,D*37*S+FVector(0,0,1),D*Rand(R,65,105)*S+FVector(0,0,-2),12*S,1.6f*S,8);
    }
    for(int J=0;J<13;++J)
    {
        const float A=J*2.39996f+Variant*.55f;
        const FVector Dir(FMath::Cos(A),FMath::Sin(A),0),Side(-Dir.Y,Dir.X,0);
        const float Z=260+J*27+Rand(R,-16,16);
        const FVector A0=Trunk(Z);
        const FVector A1=A0+Dir*Rand(R,125,185)*S+FVector(0,0,Rand(R,45,100)*S);
        const FVector A2=A0+Dir*(Rand(R,280,385)-(Z-260)*.3f)*S+Side*Rand(R,-55,55)*S+FVector(0,0,Rand(R,180,265)*S);
        Branch(M,A0,A1,A2,Rand(R,7,11)*S,1.7f*S,12);
        for(int K=0;K<5;++K)
        {
            const float T=.22f+K*.17f,Inv=1-T;const FVector Fork=A0*Inv*Inv+A1*2*Inv*T+A2*T*T;
            const FVector End=Fork+Dir*Rand(R,60,125)*S+Side*(K%2?1:-1)*Rand(R,55,130)*S+FVector(0,0,Rand(R,45,155)*S);
            const FVector Bend=FMath::Lerp(Fork,End,.5f)+FVector(0,0,-20*S);
            Branch(M,Fork,Bend,End,2.7f*S,.55f*S,7);
            for(int Tw=0;Tw<3;++Tw)
            {
                const FVector Tip=End+Dir*Rand(R,-20,50)*S+Side*Rand(R,-48,48)*S+FVector(0,0,Rand(R,8,58)*S);
                const FVector FineStart=FMath::Lerp(Bend,End,.3f),FineBend=FMath::Lerp(End,Tip,.5f);
                Branch(M,FineStart,FineBend,Tip,.8f*S,.12f*S,5);
                if(R.FRand()<.11f)continue; // exposed twigs and gaps in an autumn canopy
                for(int Layer=0;Layer<3;++Layer)
                {
                    const float Tt=.4f+Layer*.3f,Ti=1-Tt;
                    const FVector Anchor=FineStart*Ti*Ti+FineBend*2*Ti*Tt+Tip*Tt*Tt;
                    const FVector Direction=(Tip-FineStart).GetSafeNormal()+Side*(Layer%2==0?-.8f:.8f)+FVector(0,0,Rand(R,-.35f,.4f));
                    LeafSprig(M,FallKey(R,Variant),ETreeKind::Maple,Anchor,Direction,R,Rand(R,45,65)*S,12,Origins);
                }
            }
        }
    }
    // Irregular central forks round out the crown; the top does not grow into a spiky stack.
    FVector Leader=TrunkPoints.Last();
    for(int J=0;J<4;++J)
    {
        const FVector Tip=Leader+FVector(Rand(R,-135,135),Rand(R,-135,135),Rand(R,45,110))*S;
        const FVector Bend=FMath::Lerp(Leader,Tip,.5f)+FVector(12*S,0,0);
        Branch(M,Leader,Bend,Tip,(3.5f-J*.7f)*S,(2.7f-J*.7f)*S,8);
        for(int K=0;K<12;++K)
        {
            const float T=.25f+K/11.f*.75f,Inv=1-T;
            const FVector Anchor=Leader*Inv*Inv+Bend*2*Inv*T+Tip*T*T;
            const float A=K*2.39996f;const FVector Direction(FMath::Cos(A),FMath::Sin(A),Rand(R,-.3f,.5f));
            LeafSprig(M,FallKey(R,Variant),ETreeKind::Maple,Anchor,Direction,R,Rand(R,45,70)*S,12,Origins);
        }
    }
    Col.Add(FBox(FVector(-Rt,-Rt,0),FVector(Rt,Rt,520.f)));
}

void OakTree(FHallMesh& M,int Variant,TArray<FBox>& Col,TArray<FVector>* Origins)
{
    FRandomStream R(2301+Variant*149);
    // Mature, open-grown oak: buttress roots, a stout trunk and spreading, crooked limbs.
    const float Height=940.f+Variant*26.f;
    auto Trunk=[&](float Z){return FVector(FMath::Sin(Z*.007f+Variant)*19,FMath::Sin(Z*.005f)*13,Z);};
    TArray<FVector> Points;TArray<float> Radii;
    for(int J=0;J<13;++J)
    {
        const float T=J/12.f;Points.Add(Trunk(T*Height*.76f));
        Radii.Add(J==0?76.f:FMath::Lerp(54.f,7.f,FMath::Pow(T,.72f)));
    }
    Tube(M,Points,Radii,24);
    for(int J=0;J<9;++J)
    {
        const float A=J*2*PI/9+Variant*.3f;const FVector D(FMath::Cos(A),FMath::Sin(A),0);
        Branch(M,D*18+FVector(0,0,48),D*70+FVector(0,0,6),D*Rand(R,115,155)+FVector(0,0,-4),22,2.1f,10);
    }
    for(int J=0;J<11;++J)
    {
        const float A=J*2.39996f+Variant*.6f;const FVector D(FMath::Cos(A),FMath::Sin(A),0),Side(-D.Y,D.X,0);
        const FVector Start=Trunk(255+J*32.f);
        const FVector Bend=Start+D*Rand(R,180,265)+FVector(0,0,Rand(R,30,90));
        const FVector End=D*Rand(R,385,560)+Side*Rand(R,-65,65)+FVector(0,0,Rand(R,650,830));
        Branch(M,Start,Bend,End,Rand(R,14,24),2.4f,12);
        for(int K=0;K<5;++K)
        {
            const float T=.28f+K*.15f,Inv=1-T;
            const FVector Fork=Start*Inv*Inv+Bend*2*Inv*T+End*T*T;
            const FVector Tip=Fork+D*Rand(R,75,160)+Side*(K%2?1:-1)*Rand(R,70,150)+FVector(0,0,Rand(R,45,145));
            const FVector Bend2=FMath::Lerp(Fork,Tip,.55f)+Side*Rand(R,-35,35);
            Branch(M,Fork,Bend2,Tip,5.4f,.8f,8);
            for(int Tw=0;Tw<3;++Tw)
            {
                FVector LeafTip=Tip+D*Rand(R,-40,60)+Side*Rand(R,-75,75)+FVector(0,0,Rand(R,-15,55));
                LeafTip.Z=FMath::Min(LeafTip.Z,Height+20);
                const FVector FineStart=FMath::Lerp(Bend2,Tip,.45f);
                Branch(M,FineStart,Tip,LeafTip,1.2f,.16f,5);
                if(R.FRand()<.13f)continue;
                for(int Layer=0;Layer<3;++Layer)
                {
                    const int Palette=R.FRand()<.55f?0:R.FRand()<.6f?1:2;
                    const float Tt=.4f+Layer*.3f,Ti=1-Tt;
                    const FVector Anchor=FineStart*Ti*Ti+Tip*2*Ti*Tt+LeafTip*Tt*Tt;
                    const FVector Direction=(LeafTip-FineStart).GetSafeNormal()+Side*(Layer%2==0?-.8f:.8f)+D*.35f+FVector(0,0,Rand(R,-.4f,.4f));
                    LeafSprig(M,SpeciesLeaf(ETreeKind::Oak,Palette),ETreeKind::Oak,Anchor,Direction,R,Rand(R,50,80),14,Origins);
                }
            }
        }
    }
    // A few taller central forks fill the crown without a spherical cap.
    for(int J=0;J<5;++J)
    {
        const float A=J*2.39996f;const FVector Tip(FMath::Cos(A)*Rand(R,85,180),FMath::Sin(A)*Rand(R,85,180),Height+Rand(R,-45,30));
        const FVector Start=Trunk(600),Bend=FMath::Lerp(Start,Tip,.6f);
        Branch(M,Start,Bend,Tip,7.f,1.f,9);
        for(int K=0;K<14;++K)
        {
            const float T=.3f+K/13.f*.7f,Inv=1-T;
            const FVector Anchor=Start*Inv*Inv+Bend*2*Inv*T+Tip*T*T;
            const float LeafAngle=K*2.39996f;const FVector Direction(FMath::Cos(LeafAngle),FMath::Sin(LeafAngle),Rand(R,-.5f,.4f));
            LeafSprig(M,SpeciesLeaf(ETreeKind::Oak,K%3),ETreeKind::Oak,Anchor,Direction,R,Rand(R,55,90),14,Origins);
        }
    }
    Col.Add(FBox(FVector(-53,-53,0),FVector(53,53,530)));
}

void BirchTree(FHallMesh& M,int Variant,TArray<FBox>& Col,TArray<FVector>* Origins)
{
    FRandomStream R(2501+Variant*83);
    const int Stems=Variant==1?2:1;
    for(int Stem=0;Stem<Stems;++Stem)
    {
        const FVector Offset(Stem*28-10,Stem*9,0);
        auto Trunk=[&](float Z){return Offset+FVector(FMath::Sin(Z*.004f+Variant)*16+Stem*Z*.06f,FMath::Sin(Z*.006f)*13,Z);};
        TArray<FVector> Points;TArray<float> Radii;
        for(int J=0;J<13;++J){Points.Add(Trunk(J*65.f));Radii.Add(FMath::Lerp(19.f,1.4f,J/12.f));}
        Tube(M,Points,Radii,16,BirchBark);
        for(int J=0;J<12;++J)
        {
            const float A=J*2.39996f+Stem;const FVector D(FMath::Cos(A),FMath::Sin(A),0),Side(-D.Y,D.X,0);
            const float Z=250+J*40.f;const FVector Start=Trunk(Z);
            const FVector Tip=Start+D*(250-(Z-250)*.2f)+FVector(0,0,Rand(R,45,125));
            const FVector Bend=FMath::Lerp(Start,Tip,.5f)+FVector(0,0,45);
            Branch(M,Start,Bend,Tip,4.8f,.35f,8,BirchBark);
            for(int K=0;K<5;++K)
            {
                const FVector Twig=FMath::Lerp(Bend,Tip,.22f+K*.15f);
                const FVector End=Twig+Side*(K%2?1:-1)*Rand(R,40,90)+FVector(0,0,Rand(R,-65,20));
                Branch(M,Twig,FMath::Lerp(Twig,End,.5f),End,.9f,.12f,5,BirchBark);
                for(int Layer=0;Layer<4;++Layer)
                {
                    const float Tt=.4f+Layer*.2f;
                    const FVector Anchor=FMath::Lerp(Twig,End,Tt);
                    const FVector Direction=(End-Twig).GetSafeNormal()+D*Rand(R,-.5f,.5f)+Side*(Layer-1.5f)*.6f+FVector(0,0,-.3f);
                    LeafSprig(M,SpeciesLeaf(ETreeKind::Birch,Layer==0?1:0),ETreeKind::Birch,Anchor,Direction,R,Rand(R,32,52),12,Origins);
                }
            }
        }
    }
    Col.Add(FBox(FVector(-22,-22,0),FVector(Variant==1?46.f:22.f,22,490)));
}

const TCHAR* TreeName(ETreeKind Kind)
{
    return Kind==ETreeKind::Oak?TEXT("Oak"):Kind==ETreeKind::Birch?TEXT("Birch"):TEXT("Maple");
}

void Tree(FHallMesh& M,ETreeKind Kind,int Variant,TArray<FBox>& Col,TArray<FVector>* Origins)
{
    if(Kind==ETreeKind::Oak)OakTree(M,Variant,Col,Origins);
    else if(Kind==ETreeKind::Birch)BirchTree(M,Variant,Col,Origins);
    else MapleTree(M,Variant,Col,Origins);
}

void Shrub(FHallMesh& M,int Variant,TArray<FBox>& Col)
{
    FRandomStream R(1103+Variant*87);
    for(int J=0;J<12;++J)
    {
        const float A=J*2.39996f;const FVector Dir(FMath::Cos(A),FMath::Sin(A),0);
        const FVector End=Dir*Rand(R,35,70)+FVector(Variant==1?22:0,10,Rand(R,35,83));
        const FVector Start(Rand(R,-8,8),Rand(R,-8,8),2);
        Branch(M,Start,FMath::Lerp(Start,End,.5f)+FVector(0,0,-12),End,1.5f,.2f,6);
    }
    for(int J=0;J<210;++J)
    {
        const float A=Rand(R,0,2*PI),Z=Rand(R,16,105),Radius=FMath::Sqrt(FMath::Max(0.f,1-FMath::Square((Z-53)/68)))*Rand(R,30,87);
        const FVector P(FMath::Cos(A)*Radius+(Variant==1?22:0),FMath::Sin(A)*Radius,Z);
        const FRotator Rot(Rand(R,-65,65),Rand(R,0,360),Rand(R,-40,40));const float Size=Rand(R,16,26);
        Card(M,J%7==0?BushTip:BushLeaf,P,Rot.RotateVector(FVector(Size,0,0)),Rot.RotateVector(FVector(0,0,Size)));
    }
    Col.Add(FBox(FVector(-70,-70,0),FVector(70,70,100)));
}

void FrontShrub(FHallMesh& M,int Variant,TArray<FBox>& Col)
{
    FRandomStream R(1927+Variant*173);
    const float Width=Variant==2?.93f:1.f,Height=Variant==2?1.08f:1.f;
    const FVector Offset(Variant==1?10.f:0.f,0,0);
    auto Crown=[&](float A,float Z,float Radius)
    {
        const float Envelope=FMath::Sqrt(FMath::Max(.12f,1-FMath::Square((Z-58.f)/68.f)));
        return Offset+FVector(FMath::Cos(A)*Radius*Envelope*Width,
            FMath::Sin(A)*Radius*Envelope,Z*Height);
    };
    for(int StemIndex=0;StemIndex<16;++StemIndex)
    {
        const float A=StemIndex*2.39996f+Rand(R,-.12f,.12f);
        const FVector Start(Rand(R,-9,9),Rand(R,-9,9),.8f);
        const FVector StemEnd=Crown(A,Rand(R,43,78),Rand(R,32,49));
        const FVector StemBend=FMath::Lerp(Start,StemEnd,.55f)-FVector(0,0,9);
        Branch(M,Start,StemBend,StemEnd,Rand(R,.9f,1.5f),.17f,7,FrontTwig);
        for(int TwigIndex=0;TwigIndex<12;++TwigIndex)
        {
            const float T=.27f+TwigIndex*.058f,Inv=1-T;
            const FVector Anchor=Start*Inv*Inv+StemBend*2*Inv*T+StemEnd*T*T;
            const float TwigA=A+Rand(R,-1.f,1.f);
            const float Z=TwigIndex%3==0?Rand(R,24,51):TwigIndex%3==1?Rand(R,53,80):Rand(R,82,109);
            const FVector Tip=Crown(TwigA,Z,Rand(R,72,94));
            const FVector Junction=FMath::Lerp(Anchor,Tip,.54f)+FVector(0,0,2);
            Branch(M,Anchor,FMath::Lerp(Anchor,Junction,.5f),Junction,.38f,.16f,5,FrontTwig);
            // Mostly dark mature leaves inside; lighter new growth at the perimeter.
            const int Palette=(TwigIndex%4==0||StemIndex%5==0)?1:0;
            FrontSprig(M,Junction,Tip,R,Palette);
            {
                // Small bent multi-leaf clusters fill the inner canopy without an
                // opaque core or expanding the existing hedge into the walkways.
                const FVector P=FMath::Lerp(Anchor,Tip,.76f);
                const FVector Out=(Tip-Anchor).GetSafeNormal();
                const FVector U=FVector::CrossProduct(Out,FVector::UpVector).GetSafeNormal();
                const FVector V=(Out*.4f+FVector::UpVector*.9f).GetSafeNormal();
                Card(M,FrontLeaf(TwigIndex%4==0?1:2,Palette),P,U*Rand(R,9,14),V*Rand(R,10,15));
            }
        }
    }
    // Identical to the old shrub blocker: visual polish must not change routes.
    Col.Add(FBox(FVector(-70,-70,0),FVector(70,70,100)));
    UE_LOG(LogTemp,Display,TEXT("FRONT_BUSH_MESH variant=%d tris=%d curvedLeaves=2304 rootedStems=16"),Variant,M.NumTriangles());
}

void GrassClump(FHallMesh& M,bool Dry,int Variant)
{
    FRandomStream R(1211+Variant*53+(Dry?11:0));
    const FName Key=Dry?DryGrass:GreenGrass;
    for(int J=0;J<32;++J)
    {
        const float A=Rand(R,0,2*PI),H=Rand(R,Dry?18.f:13.f,Dry?31.f:24.f),W=Rand(R,.32f,.78f);
        const FVector P(Rand(R,-27,27),Rand(R,-27,27),.05f);
        const FVector U(FMath::Cos(A)*W,FMath::Sin(A)*W,0);
        const FVector Lean(FMath::Cos(A+HALF_PI)*Rand(R,3,10),FMath::Sin(A+HALF_PI)*Rand(R,3,10),0);
        const FVector Mid=P+Lean*.32f+FVector(0,0,H*.55f),Tip=P+Lean+FVector(0,0,H);
        const FVector N=(FVector(-FMath::Sin(A),FMath::Cos(A),0)*.4f+FVector::UpVector).GetSafeNormal();
        const FVector Tangent=(U-N*FVector::DotProduct(U,N)).GetSafeNormal();
        M.QuadUV(Key,P-U,P+U,Mid+U*.53f,Mid-U*.53f,N,
            FVector2D(0,1),FVector2D(1,1),FVector2D(1,.45f),FVector2D(0,.45f),Tangent);
        M.QuadUV(Key,Mid-U*.53f,Mid+U*.53f,Tip,Tip,N,
            FVector2D(0,.45f),FVector2D(1,.45f),FVector2D(.5f,0),FVector2D(.5f,0),Tangent);
    }
}

void LeafLitter(FHallMesh& M,int Variant)
{
    FRandomStream R(1301+Variant*81);
    for(int J=0;J<12;++J)
    {
        const float A=Rand(R,0,2*PI),Size=Rand(R,2.5f,5.8f);
        const FVector P(Rand(R,-32,32),Rand(R,-32,32),Rand(R,.2f,1.1f));
        const FVector U(FMath::Cos(A)*Size,FMath::Sin(A)*Size,Rand(R,-.15f,.15f)),V(-FMath::Sin(A)*Size,FMath::Cos(A)*Size,Rand(R,-.3f,.3f));
        const ETreeKind Kind=ETreeKind(J%3);
        const FName Key=Kind==ETreeKind::Maple?(Variant==0?LitterGold:Variant==2?LitterRust:Litter):SpeciesLeaf(Kind,Variant,true);
        Card(M,Key,P,U*(Kind==ETreeKind::Birch?.64f:1.f),V);
    }
}

void SingleLeaf(FHallMesh& M,int Variant,ETreeKind Kind)
{
    const FName Key=Kind==ETreeKind::Maple?(Variant==0?LitterGold:Variant==2?LitterRust:Litter):SpeciesLeaf(Kind,Variant,true);
    const float Width=Kind==ETreeKind::Birch?2.5f:Kind==ETreeKind::Oak?3.7f:5.2f;
    const float Length=Kind==ETreeKind::Birch?3.7f:6.2f;
    Card(M,Key,FVector(0,0,.15f),FVector(Width,0,.24f),FVector(0,Length,-.32f));
}

void YardDebris(FHallMesh& M,EYardDebris Kind,int Variant)
{
    FRandomStream R(3201+int(Kind)*97+Variant*37);
    if(Kind==EYardDebris::Stick)
    {
        const float Length=Variant==0?28.f:Variant==1?43.f:19.f,Radius=Variant==2?.65f:1.05f;
        const FVector A(-Length*.5f,0,Radius+.3f),B(Length*.5f,Rand(R,-2,2),Radius+.3f);
        Branch(M,A,FVector(0,Rand(R,-3,3),Radius+.9f),B,Radius,Radius*.58f,10);
        M.Cylinder(TwigCore,A,A+FVector(.12f,0,0),Radius*.91f,10,FHallUV::World(4));
        M.Cylinder(TwigCore,B-FVector(.12f,0,0),B,Radius*.51f,10,FHallUV::World(4));
        if(Variant!=2)
        {
            const FVector End(Length*.16f,7+Variant*5.f,1.1f);
            Branch(M,FVector(-Length*.12f,0,1.2f),FVector(2,4,1.8f),End,.55f,.18f,7);
        }
        // A small torn patch exposes dry wood underneath the bark.
        M.Quad(TwigCore,FVector(-3,-.3f,Radius*2.1f),FVector(5,-.2f,Radius*2.1f),
            FVector(4,.35f,Radius*2.15f),FVector(-2,.4f,Radius*2.05f),FVector::UpVector,FHallUV::World(4));
        return;
    }
    if(Kind==EYardDebris::Pinecone)
    {
        const float Length=10.5f+Variant*.8f,Center=3.45f;
        TArray<FVector> P;TArray<float> Radius;
        for(int J=0;J<9;++J)
        {
            const float T=J/8.f;P.Add(FVector(T*Length,0,Center));
            Radius.Add(.18f+2.6f*FMath::Pow(FMath::Sin(T*PI),.7f));
        }
        Tube(M,P,Radius,12,ConeCore);
        for(int Layer=0;Layer<7;++Layer)
        {
            const float T=(Layer+.65f)/8.f,X=T*Length;
            const float BodyRadius=.2f+2.5f*FMath::Pow(FMath::Sin(T*PI),.7f);
            const int Count=Layer==0||Layer==6?6:9;
            for(int K=0;K<Count;++K)
            {
                const float Angle=K*2*PI/Count+Layer*2.39996f;
                const FVector Radial(0,FMath::Cos(Angle),FMath::Sin(Angle)),Side(0,-FMath::Sin(Angle),FMath::Cos(Angle));
                const FVector Base(X,0,Center),N=(Radial+FVector(.42f,0,0)).GetSafeNormal();
                const FVector Root=Base+Radial*(BodyRadius-.4f)-FVector(.8f,0,0);
                const FVector Mid=Base+Radial*(BodyRadius+.3f),Tip=Base+Radial*(BodyRadius+.65f)+FVector(1.65f,0,0);
                const float Width=.66f+BodyRadius*.10f;
                M.QuadUV(ConeScale,Root,Mid-Side*Width,Tip,Mid+Side*Width,N,{.5f,1},{0,.5f},{.5f,0},{1,.5f},FVector(1,0,0));
                // The dark underside and overlapping raised lip give each scale real depth.
                M.Quad(ConeCore,Mid-Side*Width,Mid-Side*Width-Radial*.17f,
                    Tip-Radial*.17f,Tip,N,FHallUV::World(3));
            }
        }
        M.Cylinder(Bark,FVector(-1.4f,0,Center),FVector(.2f,0,Center),.35f,7,FHallUV::World(5));
        return;
    }
    if(Kind==EYardDebris::Paper)
    {
        // A bent, uneven scrap, including its creases and lifted corners.
        FVector P[4][4];
        for(int Y=0;Y<4;++Y)for(int X=0;X<4;++X)
            P[Y][X]=FVector((X-1.5f)*4.7f,(Y-1.5f)*3.8f,.2f+Rand(R,0,1.25f)+(X==0&&Y==3?1.4f:0));
        for(int Y=0;Y<3;++Y)for(int X=0;X<3;++X)
        {
            const FVector N=FVector::CrossProduct(P[Y][X+1]-P[Y][X],P[Y+1][X]-P[Y][X]).GetSafeNormal();
            M.QuadUV(ScrapPaper,P[Y][X],P[Y][X+1],P[Y+1][X+1],P[Y+1][X],N,
                {X/3.f,Y/3.f},{(X+1)/3.f,Y/3.f},{(X+1)/3.f,(Y+1)/3.f},{X/3.f,(Y+1)/3.f},FVector(1,0,0));
        }
        return;
    }
    if(Kind==EYardDebris::Can)
    {
        constexpr int Rings=7,Sides=14;
        FVector P[Rings+1][Sides+1];
        for(int J=0;J<=Rings;++J)for(int K=0;K<=Sides;++K)
        {
            const float A=K*2*PI/Sides,T=J/float(Rings);
            const float Dent=1-.22f*FMath::Sin(A*3+J*.8f)*FMath::Sin(T*PI);
            P[J][K]=FVector(T*8.5f+.18f*FMath::Sin(A*2+J),FMath::Cos(A)*2.9f*Dent,2.15f+FMath::Sin(A)*1.8f*Dent);
        }
        for(int J=0;J<Rings;++J)for(int K=0;K<Sides;++K)
        {
            const float A=(K+.5f)*2*PI/Sides;const FVector N(0,FMath::Cos(A),FMath::Sin(A)*1.5f);
            M.QuadUV(CanPaint,P[J][K],P[J+1][K],P[J+1][K+1],P[J][K+1],N.GetSafeNormal(),
                {K/float(Sides),J/float(Rings)},{K/float(Sides),(J+1)/float(Rings)},
                {(K+1)/float(Sides),(J+1)/float(Rings)},{(K+1)/float(Sides),J/float(Rings)},FVector(1,0,0));
        }
        for(int End:{0,Rings})
        {
            const FVector Center(End/float(Rings)*8.5f,0,2.15f),N=End==0?FVector(-1,0,0):FVector(1,0,0);
            for(int K=0;K<Sides;++K)M.Quad(CanMetal,Center,P[End][K],P[End][K+1],Center,N,FHallUV::World(5));
        }
        // Small pull tab and the dark drinking opening remain visible on the crushed end.
        M.Cylinder(ConeCore,FVector(-.1f,0,2.6f),FVector(-.2f,0,2.6f),.65f,10,FHallUV::World(2));
        M.Cylinder(CanMetal,FVector(-.25f,0,1.65f),FVector(-.4f,0,1.65f),.55f,10,FHallUV::World(2));
        return;
    }
    // An open cup lying on its side; a thin rim and dirty interior distinguish it from a cylinder.
    constexpr int Sides=18;
    for(int K=0;K<Sides;++K)
    {
        const float A=K*2*PI/Sides,B=(K+1)*2*PI/Sides;
        const FVector U(0,FMath::Cos(A),FMath::Sin(A)),V(0,FMath::Cos(B),FMath::Sin(B)),C(0,0,4.6f);
        const FVector N=(U+V-FVector(.16f,0,0)).GetSafeNormal();
        M.Quad(CupPaint,C+U*2.8f,C+V*2.8f,C+FVector(11,0,0)+V*4.45f,C+FVector(11,0,0)+U*4.45f,N,FHallUV::World(12));
        M.Quad(ScrapPaper,C+U*2.63f+FVector(.2f,0,0),C+FVector(11,0,0)+U*4.28f,
            C+FVector(11,0,0)+V*4.28f,C+V*2.63f+FVector(.2f,0,0),-N,FHallUV::World(9));
        M.Cylinder(ScrapPaper,C+FVector(11,0,0)+U*4.38f,C+FVector(11,0,0)+V*4.38f,.14f,5,FHallUV::World(9));
    }
    M.Cylinder(ScrapPaper,FVector(0,0,4.6f),FVector(.18f,0,4.6f),2.8f,18,FHallUV::World(9));
}

void ScatterYardDebris(TArray<FTransform>& Sticks,TArray<FTransform>& Pinecones,
    TArray<FTransform>& Paper,TArray<FTransform>& Cans,TArray<FTransform>& Cups)
{
    FRandomStream R(202610119);
    const FVector2D Pockets[]={{-2250,-3150},{-1350,-3500},{-950,-2860},{700,-2910},{1760,-3550},{2500,-3170}};
    auto Add=[&](TArray<FTransform>& Items,const FVector2D& Center,float Radius)
    {
        for(int Attempt=0;Attempt<35;++Attempt)
        {
            const float X=Center.X+Rand(R,-Radius,Radius),Y=Center.Y+Rand(R,-Radius,Radius);
            if(!Lawn(X,Y)||FMath::Abs(X)<520||Y<-3820||Y>-2550||Turf(X,Y)>.49f)continue;
            const float Scale=Rand(R,.82f,1.22f);
            Items.Add(FTransform(FRotator(0,Rand(R,0,360),0),FVector(X,Y,Surface(X,Y)+.35f),FVector(Scale)));
            return;
        }
    };
    for(const FVector2D& P:Pockets)
    {
        for(int J=0;J<3;++J)Add(Sticks,P,135);
        for(int J=0;J<2;++J)Add(Pinecones,P,100);
    }
    for(const FVector2D& P:{FVector2D(-750,-2730),FVector2D(710,-3090)})
    {
        Add(Paper,P,100);Add(Cans,P,95);Add(Cups,P,115);
    }
    UE_LOG(LogTemp,Display,TEXT("FRONT_YARD_DEBRIS sticks=%d pinecones=%d paper=%d cans=%d cups=%d; sparse, instanced and non-blocking"),
        Sticks.Num(),Pinecones.Num(),Paper.Num(),Cans.Num(),Cups.Num());
}

bool IsLeafGround(float X,float Y)
{
    using namespace HouseExteriorSite;
    if(Lawn(X,Y))return true;
    // Leaves can collect on the existing walks and mulch without changing their collision.
    return In(X,Y,-1400,-497,-2510,-2236) || In(X,Y,423,1340,-2510,-2236)
        || In(X,Y,-440,375,FrontSidewalkInnerY+10,-2200)
        || In(X,Y,-420,350,-2280,-2120)
        || In(X,Y,-1400,1340,-2160,-1025)
        || In(X,Y,-3540,-1425,-2375,-2210)
        || In(X,Y,1365,3330,-2305,-2140)
        || In(X,Y,-3540,-3410,-2200,1700)
        || In(X,Y,3350,3530,FrontSidewalkInnerY+10,1700)
        || In(X,Y,-3540,3530,1550,1750);
}

float ApproximateGroundHeight(float X,float Y){return Surface(X,Y);}

void Scatter(TArray<FTransform>& Green,TArray<FTransform>& Dry,TArray<FTransform>& Leaves,
    const TArray<FTransform>& BigTrees,const TArray<FTransform>& SmallTrees,const TArray<FTransform>& Shrubs)
{
    FRandomStream R(202610101); // Independent: site/vehicles/trees keep their existing placement.
    for(float X=-7900;X<8100;X+=47)for(float Y=-6150;Y<6000;Y+=47)
    {
        const float PX=X+Rand(R,-22,22),PY=Y+Rand(R,-22,22);
        if(!Lawn(PX,PY))continue;
        const float Cover=Turf(PX,PY);
        if(R.FRand()>FMath::Lerp(.04f,.98f,FMath::SmoothStep(.32f,.59f,Cover)))continue;
        const bool bDry=Cover<.51f || R.FRand()<.13f;
        const float S=Rand(R,.7f,1.35f);
        (bDry?Dry:Green).Add(FTransform(FRotator(Rand(R,-3,3),Rand(R,0,360),0),FVector(PX,PY,Surface(PX,PY)+.15f),FVector(S,S,S*Rand(R,.7f,1.12f))));
    }
    auto Under=[&](const TArray<FTransform>& Trees,int Count,float Radius)
    {
        for(const FTransform& T:Trees)
        {
            const FVector P=T.GetTranslation();
            if(FMath::Abs(P.X)>8500||P.Y < -6150||P.Y>6100)continue;
            for(int J=0;J<Count;++J)
            {
                const float A=Rand(R,0,2*PI),D=FMath::Sqrt(R.FRand())*Radius;
                const float X=P.X+FMath::Cos(A)*D,Y=P.Y+FMath::Sin(A)*D;
                if(!Lawn(X,Y))continue;
                const float S=Rand(R,.65f,1.35f);
                Leaves.Add(FTransform(FRotator(0,Rand(R,0,360),0),FVector(X,Y,Surface(X,Y)+.4f),FVector(S)));
            }
        }
    };
    Under(BigTrees,155,425);Under(SmallTrees,85,300);Under(Shrubs,12,130);
    // Sparse windblown leaves scattered across the exposed lawn as well.
    for(int J=0;J<1500;++J)
    {
        const float X=Rand(R,-4200,4200),Y=Rand(R,-3900,4500);
        if(!Lawn(X,Y))continue;
        Leaves.Add(FTransform(FRotator(0,Rand(R,0,360),0),FVector(X,Y,Surface(X,Y)+.4f),FVector(Rand(R,.65f,1.f))));
    }
    UE_LOG(LogTemp,Display,TEXT("AUTUMN_NATURE scatter green=%d dry=%d leaf_patches=%d; decorative foliage has no collision"),Green.Num(),Dry.Num(),Leaves.Num());
}
}
