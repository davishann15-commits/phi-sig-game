#include "HouseExteriorNature.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseExteriorSiteLayout.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace HouseNature
{
namespace
{
const FName Bark = TEXT("EX_NatureBark"), BushLeaf = TEXT("EX_NatureShrub"), BushTip = TEXT("EX_NatureShrubTips");
const FName Gold = TEXT("EX_NatureGold"), Copper = TEXT("EX_NatureCopper"), Rust = TEXT("EX_NatureRust"), GreenLeaf = TEXT("EX_NatureGreenLeaf");
const FName GreenGrass = TEXT("EX_NatureGrass"), DryGrass = TEXT("EX_NatureDryGrass"), Litter = TEXT("EX_NatureLitter");
const FName LitterGold = TEXT("EX_NatureLitterGold"), LitterRust = TEXT("EX_NatureLitterRust");
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
    return Z + .8f;
}
bool OnBank(float X, float Y) { return FMath::Abs(X) > 1545 && FMath::Abs(X) < 3420 && Y > 1795 && Y < 4270; }
float Surface(float X, float Y) { return OnBank(X, Y) ? Bank(X, Y) : Grade(Y); }

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

void PaintLeaf(FHallImage& I,const FVector2D& Base,const FVector2D& Tip,float Width,const FLinearColor& C,bool Maple)
{
    const FVector2D Axis=Tip-Base, Side(-Axis.Y,Axis.X);
    TArray<FVector2D> Shape;
    auto P=[&](float U,float V){return Base+Axis*V+Side.GetSafeNormal()*Width*U;};
    if(Maple)
    {
        const FVector2D Outline[]={{0,0},{-.16f,.2f},{-.62f,.23f},{-.39f,.40f},{-1,.45f},{-.66f,.63f},{-.78f,.82f},{-.25f,.69f},{0,1},{.25f,.69f},{.78f,.82f},{.66f,.63f},{1,.45f},{.39f,.4f},{.62f,.23f},{.16f,.2f}};
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

void PaintSpray(FHallImage& I,int Kind,bool Single)
{
    constexpr int W=512;
    const FLinearColor C=LeafColour(Kind);
    I.Init(W,W,FLinearColor(C.R,C.G,C.B,0));
    if(Single) PaintLeaf(I,FVector2D(256,474),FVector2D(256,45),175,C,true);
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
            PaintLeaf(I,End-FVector2D(S*18,-2),End+FVector2D(S*Rand(R,22,35),Rand(R,-32,-12)),Rand(R,25,42),Tone,Kind<4);
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

void Tube(FHallMesh& M,const TArray<FVector>& Points,const TArray<float>& Radii,int Sides)
{
    if(Points.Num()<2||Points.Num()!=Radii.Num())return;
    FHallSection& S=M.Sec(Bark);
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

void Branch(FHallMesh& M,const FVector& A,const FVector& Bend,const FVector& B,float R0,float R1,int Sides=8)
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
    Tube(M,P,Radius,Sides);
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
    HallMakeTextured(L,Bark,B,BN,.94f,FLinearColor(.62f,.54f,.44f),.85f);

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
    const FName Keys[]={Gold,Copper,Rust,GreenLeaf,BushLeaf};
    for(int K=0;K<5;++K){PaintSpray(I,K,false);Plant(Keys[K],I,K<4?2.4f:.6f,.7f);}
    PaintSpray(I,4,false);Plant(BushTip,I,.9f,.7f);
    if(UMaterialInstanceDynamic* Tips=Cast<UMaterialInstanceDynamic>(L->Mat(BushTip)))Tips->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1.25f,1.12f,.73f));
    PaintSpray(I,1,true);Plant(Litter,I,0.f,.08f);
    PaintSpray(I,0,true);Plant(LitterGold,I,0.f,.08f);
    PaintSpray(I,2,true);Plant(LitterRust,I,0.f,.08f);
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
    UE_LOG(LogTemp,Display,TEXT("AUTUMN_NATURE materials ready: bark, patchy turf, veined fall leaves, shrubs, grass, litter"));
}

void Tree(FHallMesh& M,bool Big,int Variant,TArray<FBox>& Col)
{
    FRandomStream R(901+Variant*131+(Big?0:41));
    const float S=Big?1.f:.71f,Rt=Big?30.f:18.f;
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
                Branch(M,FMath::Lerp(Bend,End,.3f),FMath::Lerp(End,Tip,.5f),Tip,.8f*S,.12f*S,5);
                if(R.FRand()<.11f)continue; // exposed twigs and gaps in an autumn canopy
                for(int Layer=0;Layer<5;++Layer)
                {
                    const FVector P=FMath::Lerp(Bend,Tip,Rand(R,.35f,1.1f))+FVector(Rand(R,-35,35),Rand(R,-35,35),Rand(R,-15,40))*S;
                    const float Yaw=Rand(R,0,2*PI),Tilt=Rand(R,-.65f,.7f),Size=Rand(R,32,51)*S;
                    const FVector U(FMath::Cos(Yaw),FMath::Sin(Yaw),0),V(-FMath::Sin(Yaw)*FMath::Sin(Tilt),FMath::Cos(Yaw)*FMath::Sin(Tilt),FMath::Cos(Tilt));
                    Card(M,FallKey(R,Variant),P,U*Size,V*Size);
                }
            }
        }
    }
    // Upper crown includes a narrow leader, avoiding a flat mushroom silhouette.
    FVector Leader=TrunkPoints.Last();
    for(int J=0;J<4;++J)
    {
        const FVector Tip=Leader+FVector(Rand(R,-70,70),Rand(R,-70,70),Rand(R,55,90))*S;
        Branch(M,Leader,FMath::Lerp(Leader,Tip,.5f)+FVector(12*S,0,0),Tip,(3.5f-J*.7f)*S,(2.7f-J*.7f)*S,8);
        for(int K=0;K<23;++K)
        {
            const FVector P=Tip+FVector(Rand(R,-85,85),Rand(R,-85,85),Rand(R,-20,35))*S;
            const FRotator Rot(Rand(R,-55,55),Rand(R,0,360),Rand(R,-30,30));
            Card(M,FallKey(R,Variant),P,Rot.RotateVector(FVector(40*S,0,0)),Rot.RotateVector(FVector(0,0,42*S)));
        }
        Leader=Tip;
    }
    Col.Add(FBox(FVector(-Rt,-Rt,0),FVector(Rt,Rt,Big?520.f:360.f)));
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
        Card(M,Variant==0?LitterGold:Variant==2?LitterRust:Litter,P,U,V);
    }
}

void SingleLeaf(FHallMesh& M,int Variant)
{
    Card(M,Variant==0?LitterGold:Variant==2?LitterRust:Litter,
        FVector(0,0,.15f),FVector(5.2f,0,.15f),FVector(0,6.2f,-.25f));
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
