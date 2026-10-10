#include "HouseExteriorCars.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace HouseCars
{
namespace
{
const FName Rubber=TEXT("EX_CarTyre"),Trim=TEXT("EX_CarTrim"),Chrome=TEXT("EX_CarChrome"),
    Glass=TEXT("EX_CarWindow"),Headlamp=TEXT("EX_CarHeadlamp"),Tail=TEXT("EX_CarTail"),
    Plate=TEXT("EX_CarPlate"),Brake=TEXT("EX_CarBrake");
const FHallUV UV=FHallUV::World(100);
// The old parking props were nearly nine metres long. Keep their parking
// centres, with normal vehicle proportions and round, full-size wheels.
constexpr float BodyScale=.56f;

void Box(FHallMesh& M,FName K,const FVector& A,const FVector& B){M.BoxMinMax(K,A,B,UV);}
void Face(FHallMesh& M,FName K,const FVector& A,const FVector& B,const FVector& C,const FVector& D,const FVector& N)
{M.Quad(K,A,B,C,D,N,UV);}
void Rail(FHallMesh& M,FName K,const FVector& A,const FVector& B,float R=1.f)
{M.Cylinder(K,A,B,R,6,UV,true);}

float Top(float X)
{
    const float Ax=FMath::Abs(X);
    return 103.f-FMath::Pow(Ax/435.f,3.f)*18.f;
}
float Width(float X){return 177.f-FMath::Pow(FMath::Abs(X)/435.f,4.f)*25.f;}
float ArchFloor(float X)
{
    float Z=37.f;
    for(float C:{-285.f,285.f})
    {
        const float DX=(X-C)*BodyScale;
        if(FMath::Abs(DX)<48.f)Z=FMath::Max(Z,36.f+FMath::Sqrt(48.f*48.f-DX*DX));
    }
    return Z;
}

void Wheel(FHallMesh& M,float X,float Y)
{
    const FVector C(X,Y,37.f);
    constexpr int32 Sides=28,Rings=7;
    const float Across[Rings]={-12,-11,-8,0,8,11,12};
    const float Radii[Rings]={26,34,36.5f,37,36.5f,34,26};
    FHallSection& Sec=M.Sec(Rubber);const int32 Base=Sec.V.Num();
    for(int32 J=0;J<Rings;++J)for(int32 K=0;K<=Sides;++K)
    {
        const float A=K*2*PI/Sides;
        const FVector D(FMath::Cos(A),0,FMath::Sin(A));
        const float L=J==0?-1.f:J==Rings-1?1.f:Across[J]/12.f;
        Sec.V.Add(C+D*Radii[J]+FVector(0,Across[J],0));
        Sec.N.Add((D+FVector(0,L*.9f,0)).GetSafeNormal());
        Sec.UV.Add(FVector2D(K/float(Sides)*8.f,J/float(Rings-1)));
        Sec.T.Add(FProcMeshTangent(FVector(-FMath::Sin(A),0,FMath::Cos(A)),false));
    }
    for(int32 J=0;J+1<Rings;++J)for(int32 K=0;K<Sides;++K)
    {
        const int32 A=Base+J*(Sides+1)+K,B=A+1,Cc=B+Sides+1,D=A+Sides+1;
        const bool Front=FVector::DotProduct(FVector::CrossProduct(Sec.V[B]-Sec.V[A],Sec.V[Cc]-Sec.V[A]),Sec.N[A])<0;
        if(Front)Sec.I.Append({A,B,Cc,A,Cc,D});else Sec.I.Append({A,Cc,B,A,D,Cc});
    }
    const float Sign=FMath::Sign(Y),Outer=Y+Sign*12.2f;
    M.Cylinder(Brake,FVector(X,Outer-Sign*4,37),FVector(X,Outer,37),24,24,UV,true);
    // A narrow rim lip, five shaped spokes, centre cap, and five lug nuts.
    for(int32 K=0;K<28;++K)
    {
        const float A=K*2*PI/28,B=(K+1)*2*PI/28;
        Rail(M,Chrome,FVector(X+FMath::Cos(A)*24,Outer+Sign*.3f,37+FMath::Sin(A)*24),
            FVector(X+FMath::Cos(B)*24,Outer+Sign*.3f,37+FMath::Sin(B)*24),.85f);
    }
    for(int32 K=0;K<5;++K)
    {
        const float A=K*2*PI/5+.28f;
        const FVector D(FMath::Cos(A),0,FMath::Sin(A)),U(-FMath::Sin(A),0,FMath::Cos(A));
        const FVector P(X,Outer+Sign*.8f,37);
        Face(M,Chrome,P+D*7-U*2.7f,P+D*23-U*1.6f,P+D*23+U*1.6f,P+D*7+U*2.7f,FVector(0,Sign,0));
        M.Cylinder(Chrome,P+D*4-FVector(0,Sign*.2f,0),P+D*4+FVector(0,Sign*.5f,0),.7f,6,UV,true);
    }
    M.Cylinder(Chrome,FVector(X,Outer,37),FVector(X,Outer+Sign*1.1f,37),6,16,UV,true);
    // Low-profile tread grooves catch highlights without needing textured physics tyres.
    for(int32 K=0;K<28;++K)
    {
        const float A=K*2*PI/28;
        const FVector D(FMath::Cos(A),0,FMath::Sin(A)),T(-FMath::Sin(A),0,FMath::Cos(A));
        Face(M,Trim,C+D*37.15f+FVector(0,-7,0)-T*.45f,C+D*37.15f+FVector(0,7,0)-T*.45f,
            C+D*37.15f+FVector(0,7,0)+T*.45f,C+D*37.15f+FVector(0,-7,0)+T*.45f,D);
    }
}

void Body(FHallMesh& M,FName Paint)
{
    // Side skin stops at the wheel arches. Its lower surface does not cover the tyres.
    TArray<float> Xs={-435,-412,-365,-210,-130,0,130,210,365,412,435};
    for(float C:{-285.f,285.f})for(int32 K=0;K<=16;++K)Xs.Add(C+48.f/BodyScale*FMath::Cos(K*PI/16));
    Xs.Sort();
    for(int32 J=0;J+1<Xs.Num();++J)
    {
        const float A=Xs[J],B=Xs[J+1];if(B-A<.01f)continue;
        const float YA=Width(A),YB=Width(B),ZA=Top(A),ZB=Top(B);
        for(float S:{-1.f,1.f})
        {
            Face(M,Paint,FVector(A,S*YA,ArchFloor(A)),FVector(B,S*YB,ArchFloor(B)),
                FVector(B,S*YB,ZB-8),FVector(A,S*YA,ZA-8),FVector(0,S,.1f).GetSafeNormal());
            Face(M,Paint,FVector(A,S*YA,ZA-8),FVector(B,S*YB,ZB-8),
                FVector(B,S*YB*.88f,ZB),FVector(A,S*YA*.88f,ZA),FVector(0,S*.7f,.7f).GetSafeNormal());
        }
        for(int32 K=0;K<4;++K)
        {
            const float V0=-.88f+K*.44f,V1=V0+.44f;
            auto P=[&](float X,float W,float Z,float V){return FVector(X,W*V,Z+(1-FMath::Square(V/.88f))*2.f);};
            const FVector N(0,(V0+V1)*.12f,1);
            Face(M,Paint,P(A,YA,ZA,V0),P(B,YB,ZB,V0),P(B,YB,ZB,V1),P(A,YA,ZA,V1),N.GetSafeNormal());
        }
    }
    Box(M,Trim,FVector(-420,-147,30),FVector(420,147,37));
    for(float X:{-435.f,435.f})
    {
        const float S=FMath::Sign(X),W=Width(X),Z=Top(X);
        Face(M,Paint,FVector(X,-W,42),FVector(X,W,42),FVector(X,W*.88f,Z),FVector(X,-W*.88f,Z),FVector(S,0,0));
        Rail(M,Trim,FVector(X+S*1,-W+8,48),FVector(X+S*1,W-8,48),2.2f);
    }
    for(float S:{-1.f,1.f})
    {
        Rail(M,Trim,FVector(-226,S*179,39),FVector(226,S*179,39),1.6f);
        for(float C:{-285.f,285.f})for(int32 K=0;K<16;++K)
        {
            const float A=K*PI/16,B=(K+1)*PI/16;
            const float X0=C+48.7f/BodyScale*FMath::Cos(A),X1=C+48.7f/BodyScale*FMath::Cos(B);
            Rail(M,Paint,FVector(X0,S*(Width(X0)+.4f),36+48.7f*FMath::Sin(A)),
                FVector(X1,S*(Width(X1)+.4f),36+48.7f*FMath::Sin(B)),1.4f);
        }
    }
}

void Cabin(FHallMesh& M,FName Paint,int32 Style)
{
    const float Back=Style==2?-28.f:-250.f,RearTop=Style==2?12.f:Style==1?-214.f:-138.f;
    const float Front=237,FrontTop=108,H=Style==1?184.f:Style==2?173.f:160.f;
    for(float S:{-1.f,1.f})
    {
        const FVector A(Back,S*147,102),B(RearTop,S*126,H-3),C(FrontTop,S*126,H-3),D(Front,S*147,102);
        const FVector N=FVector(0,S,21.f/(H-105.f)).GetSafeNormal();
        Face(M,Paint,A,B,C,D,N);
        const float Split=Style==2?95.f:-26.f;
        const FVector Centre=(A+B+C+D)*.25f;
        const FVector A0=FMath::Lerp(A,Centre,.13f)+N*.45f,B0=FMath::Lerp(B,Centre,.13f)+N*.45f,
            C0=FMath::Lerp(C,Centre,.13f)+N*.45f,D0=FMath::Lerp(D,Centre,.13f)+N*.45f;
        const FVector Lo=FMath::Lerp(A0,D0,(Split-A0.X)/(D0.X-A0.X)),
            Hi=FMath::Lerp(B0,C0,(Split-B0.X)/(C0.X-B0.X));
        Face(M,Glass,A0,B0,Hi-FVector(4,0,0),Lo-FVector(4,0,0),N);
        Face(M,Glass,Lo+FVector(4,0,0),Hi+FVector(4,0,0),C0,D0,N);
        Rail(M,Trim,A0,D0,1);Rail(M,Trim,B0,C0,1);Rail(M,Trim,A0,B0,1);Rail(M,Trim,C0,D0,1);
        Rail(M,Trim,Lo,Hi,2.f);
        // Door outlines follow the upper belt and stop above the sill.
        for(float X:{Back+5,Split,Front-10})Rail(M,Trim,FVector(X,S*(Width(X)+.35f),47),FVector(X,S*(Width(X)+.35f),96),.48f);
        for(float X:{Split-35,Front-54})
        {
            const float Y=S*(Width(X)+1.8f);
            Box(M,Trim,FVector(X-8,FMath::Min(Y-S*.4f,Y+S*.4f),84),FVector(X+8,FMath::Max(Y-S*.4f,Y+S*.4f),88));
            Rail(M,Chrome,FVector(X-6,Y+S*.5f,87),FVector(X+6,Y+S*.5f,87),.8f);
        }
        // Separate mirror stalk, painted shell and reflective face.
        Rail(M,Trim,FVector(177,S*151,114),FVector(172,S*197,114),2.1f);
        Box(M,Paint,FVector(158,FMath::Min(S*191,S*209),111),FVector(186,FMath::Max(S*191,S*209),122));
        Face(M,Glass,FVector(157,S*192,112),FVector(157,S*208,112),FVector(157,S*208,120),FVector(157,S*192,120),FVector(-1,0,0));
    }
    for(int32 K=0;K<6;++K)
    {
        const float Y0=-126+K*42.f,Y1=Y0+42;
        auto Z=[&](float Y){return H+2.f*(1-FMath::Square(Y/126))-3.f*FMath::Square(Y/126);};
        Face(M,Paint,FVector(RearTop,Y0,Z(Y0)),FVector(FrontTop,Y0,Z(Y0)),
            FVector(FrontTop,Y1,Z(Y1)),FVector(RearTop,Y1,Z(Y1)),FVector(0,(Y0+Y1)*.002f,1).GetSafeNormal());
    }
    for(bool IsFront:{false,true})
    {
        const float Low=IsFront?Front:Back,High=IsFront?FrontTop:RearTop;
        const FVector N=FVector(IsFront?H-106:106-H,0,FMath::Abs(Low-High)).GetSafeNormal();
        const FVector A(Low,-147,103),B(High,-126,H-3),C(High,126,H-3),D(Low,147,103);
        Face(M,Paint,A,B,C,D,N);
        const FVector Centre=(A+B+C+D)*.25f;
        Face(M,Glass,FMath::Lerp(A,Centre,.10f)+N*.3f,FMath::Lerp(B,Centre,.10f)+N*.3f,
            FMath::Lerp(C,Centre,.10f)+N*.3f,FMath::Lerp(D,Centre,.10f)+N*.3f,N);
        if(IsFront)
        {
            for(float Y:{-62.f,42.f})Rail(M,Trim,FVector(Low-14,Y-43,110),FVector(Low-34,Y+26,118),.7f);
        }
    }
    if(Style==1)
        for(float S:{-1.f,1.f})Rail(M,Trim,FVector(RearTop+12,S*117,H+3),FVector(FrontTop-12,S*117,H+3),2.1f);
    if(Style==2)
    {
        Box(M,Trim,FVector(-385,-126,103),FVector(-54,126,104.4f));
        for(float S:{-1.f,1.f})Rail(M,Paint,FVector(-411,S*141,103),FVector(-38,S*141,107),4.f);
        for(float X=-373;X<-70;X+=32)Rail(M,Trim,FVector(X,-120,104.5f),FVector(X,120,104.5f),.9f);
    }
    Rail(M,Trim,FVector(RearTop+23,45,H+1),FVector(RearTop+15,45,H+18),.55f);
}

void LampsAndGrille(FHallMesh& M)
{
    for(float S:{-1.f,1.f})
    {
        Box(M,Trim,FVector(435,S<0?-145:89,62),FVector(437,S<0?-89:145,78));
        Box(M,Headlamp,FVector(437,S<0?-140:94,64),FVector(438,S<0?-94:140,76));
        for(float Y:{S*105,S*128})M.Cylinder(Headlamp,FVector(438,Y,70),FVector(439,Y,70),4.1f,12,UV,true);
        Box(M,Tail,FVector(-438,S<0?-143:101,65),FVector(-436,S<0?-101:143,80));
        Rail(M,Chrome,FVector(-438,S*104,65),FVector(-438,S*140,65),.6f);
    }
    Box(M,Trim,FVector(436,-79,52),FVector(437,79,80));
    for(int32 K=0;K<3;++K)Rail(M,Chrome,FVector(438,-76,57+K*8),FVector(438,76,57+K*8),.8f);
    for(float X:{-439.f,439.f})
    {
        Box(M,Trim,FVector(X-.5f,-28,45),FVector(X+.5f,28,60));
        Box(M,Plate,FVector(X+.55f,-25,47),FVector(X+.65f,25,58));
        for(float Y:{-17.f,-9.f,-1.f,7.f,15.f})
            Face(M,Trim,FVector(X+1,Y,49),FVector(X+1,Y+3,49),FVector(X+1,Y+3,55),FVector(X+1,Y,55),FVector(FMath::Sign(X),0,0));
    }
    M.Cylinder(Chrome,FVector(-432,-100,39),FVector(-446,-100,39),3.6f,12,UV,true);
}
}

void CreateLook(UHouseBlackHallSubsystem* L)
{
    if(!L||L->Materials.Contains(Rubber))return;
    UMaterialInterface* PaintParent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorCars/M_CarPaint.M_CarPaint"));
    UMaterialInterface* SurfaceParent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorCars/M_CarSurface.M_CarSurface"));
    if(!PaintParent||!SurfaceParent){UE_LOG(LogTemp,Error,TEXT("HOUSE_CARS missing saved car materials"));return;}
    auto Mat=[&](FName Key,FLinearColor Colour,float Rough,float Metal=0.f,FLinearColor Em=FLinearColor::Black,bool Paint=false)
    {
        UMaterialInstanceDynamic* M=UMaterialInstanceDynamic::Create(Paint?PaintParent:SurfaceParent,L);
        M->SetVectorParameterValue(TEXT("Tint"),Colour);M->SetScalarParameterValue(TEXT("Roughness"),Rough);
        M->SetScalarParameterValue(TEXT("Metallic"),Metal);M->SetVectorParameterValue(TEXT("Emission"),Em);
        L->Materials.Add(Key,M);
    };
    Mat(TEXT("EX_CarPaint0"),FLinearColor(.33f,.029f,.017f),.30f,.05f,FLinearColor::Black,true);
    Mat(TEXT("EX_CarPaint1"),FLinearColor(.64f,.66f,.69f),.28f,.03f,FLinearColor::Black,true);
    Mat(TEXT("EX_CarPaint2"),FLinearColor(.13f,.18f,.22f),.32f,.08f,FLinearColor::Black,true);
    Mat(Rubber,FLinearColor(.025f,.026f,.027f),.92f);
    Mat(Trim,FLinearColor(.012f,.014f,.016f),.58f);
    Mat(Chrome,FLinearColor(.57f,.59f,.62f),.29f,.72f);
    Mat(Brake,FLinearColor(.09f,.09f,.09f),.62f,.55f);
    Mat(Glass,FLinearColor(.028f,.047f,.075f),.14f,.08f);
    Mat(Headlamp,FLinearColor(.71f,.78f,.86f),.23f,.1f,FLinearColor(.30f,.35f,.44f));
    Mat(Tail,FLinearColor(.29f,.011f,.006f),.28f,.1f,FLinearColor(.14f,.003f,.001f));
    Mat(Plate,FLinearColor(.62f,.66f,.61f),.60f);
    UE_LOG(LogTemp,Display,TEXT("HOUSE_CARS clear-coated sedan, crossover, pickup; wheel arches, tyres/rims, glazing, mirrors and trim ready"));
}

void Build(FHallMesh& M,int32 Style,TArray<FBox>& Col)
{
    const FName Paint=*FString::Printf(TEXT("EX_CarPaint%d"),Style);
    Body(M,Paint);Cabin(M,Paint,Style);LampsAndGrille(M);
    for(auto& Pair:M.Sections)
    {
        FHallSection& S=Pair.Value;
        for(FVector& P:S.V){P.X*=BodyScale;P.Y*=BodyScale;}
        for(FVector& N:S.N)N=FVector(N.X/BodyScale,N.Y/BodyScale,N.Z).GetSafeNormal();
        for(FProcMeshTangent& T:S.T)T.TangentX=FVector(T.TangentX.X*BodyScale,T.TangentX.Y*BodyScale,T.TangentX.Z).GetSafeNormal();
    }
    for(float X:{-285.f,285.f})for(float Y:{-165.f,165.f})Wheel(M,X*BodyScale,Y*BodyScale);
    Col.Add(FBox(FVector(-435*BodyScale,-178*BodyScale,0),FVector(435*BodyScale,178*BodyScale,100)));
    const float H=Style==1?187.f:Style==2?175.f:163.f;
    Col.Add(FBox(FVector((Style==2?-28.f:-250.f)*BodyScale,-147*BodyScale,100),FVector(237*BodyScale,147*BodyScale,H)));
}
}
