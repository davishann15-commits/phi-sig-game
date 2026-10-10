#include "HouseBasketballCourt.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
const FHallUV UV = FHallUV::World(100.f), BrickUV = FHallUV::World(120.f), GrassUV = FHallUV::World(400.f);
void Box(FHallMesh& M, FName K, const FVector& A, const FVector& B, const FHallUV& U = UV) { M.BoxMinMax(K, A.ComponentMin(B), A.ComponentMax(B), U); }
void Tube(FHallMesh& M, FName K, const FVector& A, const FVector& B, float R, int32 Seg = 8) { M.Cylinder(K, A, B, R, Seg, UV); }
void Ring(FHallMesh& M, FName K, const FVector& C, float R, float TubeRadius, int32 Seg = 48)
{
    for(int32 I=0;I<Seg;++I)
    {
        float A=2*PI*I/Seg,B=2*PI*(I+1)/Seg;
        Tube(M,K,C+FVector(R*FMath::Cos(A),R*FMath::Sin(A),0),C+FVector(R*FMath::Cos(B),R*FMath::Sin(B),0),TubeRadius);
    }
}
void PaintCourt(UHouseBlackHallSubsystem* L)
{
    if(L->Materials.Contains(TEXT("EX_Court")))return;
    FHallImage Img;Img.Init(1536,1024,FLinearColor::White);
    auto P=[&](float X,float Y){return FVector2D((X+1380)/2760*Img.W,(Y-1900)/1840*Img.H);};
    for(int32 Y=0;Y<Img.H;++Y)for(int32 X=0;X<Img.W;++X)
    {
        float A=HallFbm(X*.025f,Y*.025f,4,291),G=HallNoise(X*.7f,Y*.7f,47);
        float V=.45f+.12f*A+.04f*G;
        Img.At(X,Y)=FLinearColor(V,V*.98f,V*.92f);
    }
    auto Line=[&](TArray<FVector2D> Points,float W=5.f){TArray<FVector2D> Q;for(auto& A:Points)Q.Add(P(A.X,A.Y));Img.Stroke(Q,W*Img.W/2760,FLinearColor(.022f,.024f,.027f));};
    auto Arc=[&](float X,float Y,float Radius,float Start,float Stop,float Width=5.f){TArray<FVector2D> Q;for(int I=0;I<=100;++I){float A=Start+(Stop-Start)*I/100;Q.Add(FVector2D(X+Radius*FMath::Cos(A),Y+Radius*FMath::Sin(A)));}Line(Q,Width);};
    // A narrow checker strip at each end, with worn edges rather than bright new paint.
    for(float End:{-1.f,1.f})for(int Col=0;Col<3;++Col)for(int Row=0;Row<22;++Row)
    {
        float X=End<0?-1365+Col*55:1200+Col*55,Y=1910+Row*82;
        Img.Rect(P(X,Y),P(X+54,Y+81),(Col+Row)%2?FLinearColor(.32f,.026f,.035f):FLinearColor(.78f,.77f,.72f));
    }
    Line({{-1190,1940},{1190,1940},{1190,3700},{-1190,3700},{-1190,1940}},6);
    Line({{-7,1940},{-7,3700}},4);Line({{7,1940},{7,3700}},4);
    Arc(0,HouseCourt::MidY,180,0,2*PI,5);
    for(float E:{-1.f,1.f})
    {
        Line({{E*1190,2580},{E*700,2580},{E*700,3060},{E*1190,3060}},6);
        Arc(E*700,HouseCourt::MidY,240,0,2*PI,4);
        Arc(E*1150,HouseCourt::MidY,600,E<0?-HALF_PI:HALF_PI,E<0?HALF_PI:PI*1.5f,6);
        for(float X:{850.f,1000.f})for(float Y:{2580.f,3060.f})Line({{E*X,Y-14},{E*X,Y+14}},5);
    }
    FVector2D C=P(0,HouseCourt::MidY);Img.Ellipse(C,FVector2D(118.f*Img.W/2760,118.f*Img.H/1840),0,FLinearColor(.38f,.024f,.044f));
    // Cream crest, crossed branches and fraternity monogram in the central red medallion.
    FLinearColor Cream(.80f,.78f,.68f);
    auto Crest=[&](TArray<FVector2D> Q,float W=4.f){TArray<FVector2D> A;for(auto V:Q)A.Add(P(V.X,V.Y+HouseCourt::MidY));Img.Stroke(A,W*Img.W/2760,Cream);};
    Crest({{-42,46},{-52,-43},{0,-64},{52,-43},{42,46},{0,74},{-42,46}});
    Img.Ellipse(C,FVector2D(26.f*Img.W/2760,23.f*Img.H/1840),0,Cream);
    Img.Ellipse(C,FVector2D(19.f*Img.W/2760,16.f*Img.H/1840),0,FLinearColor(.38f,.024f,.044f));
    Crest({{0,-42},{0,43}},6);
    for(float E:{-1.f,1.f}){Crest({{0,93},{E*65,54},{E*82,0},{E*58,-70}},3);for(int I=0;I<6;++I){float Y=-55+I*22;Crest({{E*(59+15*FMath::Sin(I*.5f)),Y},{E*94,Y-13}},3);}}
    // Expansion cuts and irregular cracks cross both concrete and painted lines.
    FRandomStream R(894);
    for(float X:{-860.f,-390.f,480.f,990.f})
    {
        TArray<FVector2D> Q={P(X,1900),P(X+70,3740)};Img.Stroke(Q,.85f,FLinearColor(.18f,.17f,.15f),.5f);
    }
    for(int K=0;K<30;++K)
    {
        float X=R.FRandRange(-1380,1380),Y=R.FRandRange(1900,3740),A=R.FRandRange(0,2*PI);
        TArray<FVector2D> Q;Q.Add(P(X,Y));
        for(int I=0;I< R.RandRange(5,13);++I){A+=R.FRandRange(-.6f,.6f);X+=FMath::Cos(A)*R.FRandRange(16,55);Y+=FMath::Sin(A)*R.FRandRange(16,55);Q.Add(P(X,Y));}
        Img.Stroke(Q,R.FRandRange(.5f,1.4f),FLinearColor(.055f,.05f,.043f),.8f);
    }
    for(int K=0;K<220;++K)
    {
        FVector2D At(R.FRandRange(0,Img.W),R.FRandRange(0,Img.H));
        Img.Ellipse(At,FVector2D(R.FRandRange(1,9),R.FRandRange(1,4)),R.FRandRange(0,180),FLinearColor(.40f,.39f,.35f),R.FRandRange(.08f,.25f));
    }
    UTexture2D* T=HallMakeTexture(Img,true,false,TEXT("T_ExteriorCourt"),false);L->Textures.Add(TEXT("T_ExteriorCourt"),T);
    HallMakeTextured(L,TEXT("EX_Court"),T,nullptr,.92f);
    HallMakePlain(L,TEXT("EX_CourtRim"),FLinearColor(.60f,.06f,.02f),.35f,.55f);
    HallMakePlain(L,TEXT("EX_CourtNet"),FLinearColor(.65f,.64f,.56f),.9f);
    HallMakePlain(L,TEXT("EX_CourtAC"),FLinearColor(.51f,.52f,.48f),.6f,.4f);
    HallMakePlain(L,TEXT("EX_CourtDryGrass"),FLinearColor(.19f,.20f,.075f),.97f);
    if(auto* Parent=HallHouseMat(TEXT("M_House_Glass")))
    {
        auto* M=UMaterialInstanceDynamic::Create(Parent,L);M->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.33f,.43f,.40f));M->SetScalarParameterValue(TEXT("Opacity"),.16f);M->SetScalarParameterValue(TEXT("Roughness"),.19f);L->Materials.Add(TEXT("EX_CourtGlass"),M);
    }
}
void Hoop(FHallMesh& M,FHallMesh& D,int32 End)
{
    FVector Rim=HouseCourt::Rim(End);float S=End==0?-1:1,F=HouseCourt::Floor;
    auto V=[&](float X,float Y,float Z){return FVector(S*(1150+X),HouseCourt::MidY+Y,F+Z);};
    Box(M,TEXT("EX_Black"),V(105,-78,0),V(235,78,20));
    Box(M,TEXT("EX_Grey"),V(121,-62,20),V(220,62,24));
    Tube(M,TEXT("EX_Black"),V(192,0,20),V(192,0,235),7,16);
    Tube(M,TEXT("EX_Black"),V(192,0,235),V(167,0,300),7,16);
    Tube(M,TEXT("EX_Black"),V(167,0,300),V(34,0,315),7,16);
    Tube(M,TEXT("EX_Metal"),V(190,0,180),V(34,0,295),2.2f);
    for(float Y:{-53.f,53.f})Tube(M,TEXT("EX_Black"),V(125,Y,16),V(192,0,110),3);
    for(float Y:{-61.f,61.f})Tube(M,TEXT("EX_Rubber"),V(226,Y-5,12),V(226,Y+5,12),10,16);
    // Transparent board is a real colliding surface, with a visible frame and shooting square.
    Box(M,TEXT("EX_CourtGlass"),V(29,-91,289),V(33,91,397));
    for(float Y:{-93.f,93.f})Box(M,TEXT("EX_Black"),V(26,Y-2.5f,286),V(36,Y+2.5f,400));
    for(float Z:{286.f,400.f})Box(M,TEXT("EX_Black"),V(26,-95,Z-2.5f),V(36,95,Z+2.5f));
    for(float Y:{-29.f,29.f})Box(D,TEXT("EX_White"),V(27,Y-1.6f,305),V(28,Y+1.6f,350));
    for(float Z:{305.f,350.f})Box(D,TEXT("EX_White"),V(27,-30.6f,Z-1.6f),V(28,30.6f,Z+1.6f));
    Box(M,TEXT("EX_CourtRim"),V(23,-8,302),V(30,8,310));
    Ring(M,TEXT("EX_CourtRim"),Rim,24.2f,1.25f,64);
    // Twisted diamond net. Cosmetic ropes never seal the opening or stop the ball.
    for(int I=0;I<12;++I)for(int J=0;J<5;++J)for(float Dir:{-1.f,1.f})
    {
        float A=2*PI*(I+Dir*.5f*J)/12,B=2*PI*(I+Dir*.5f*(J+1))/12;
        float R0=24-(J<3?J*3:9),R1=24-((J+1)<3?(J+1)*3:9);
        Tube(D,TEXT("EX_CourtNet"),Rim+FVector(R0*FMath::Cos(A),R0*FMath::Sin(A),-J*9.f),Rim+FVector(R1*FMath::Cos(B),R1*FMath::Sin(B),-(J+1)*9.f),.45f,5);
    }
}
void Fence(FHallMesh& M,const FVector& A,const FVector& B)
{
    FVector Dir=(B-A).GetSafeNormal2D();float Length=FVector::Dist2D(A,B);int Bays=FMath::CeilToInt(Length/450.f);
    FVector N(-Dir.Y,Dir.X,0);float Wall=210;
    auto Bar=[&](FName K,const FVector& P,const FVector& Q,float W,float H,const FHallUV& U=UV){M.Box(K,FTransform(FRotationMatrix::MakeFromYZ((Q-P).GetSafeNormal(),FVector::UpVector).ToQuat(),(P+Q)*.5f),FVector(W,(Q-P).Size()*.5f,H),U);};
    Bar(TEXT("EX_Brick"),A+FVector(0,0,Wall*.5f-20),B+FVector(0,0,Wall*.5f-20),18,Wall*.5f+20,BrickUV);
    Bar(TEXT("EX_Stone"),A+FVector(0,0,Wall),B+FVector(0,0,Wall),21,6);
    for(int I=0;I<=Bays;++I)
    {
        FVector P=FMath::Lerp(A,B,I/float(Bays));Box(M,TEXT("EX_Brick"),P+FVector(-26,-26,-40),P+FVector(26,26,328),BrickUV);Box(M,TEXT("EX_Stone"),P+FVector(-33,-33,328),P+FVector(33,33,340));
    }
    for(int I=0;I<Bays;++I)
    {
        FVector P=FMath::Lerp(A,B,I/float(Bays))+Dir*28,Q=FMath::Lerp(A,B,(I+1)/float(Bays))-Dir*28;
        for(float Z:{227.f,315.f})Bar(TEXT("EX_Black"),P+FVector(0,0,Z),Q+FVector(0,0,Z),2.3f,2.3f);
        float Span=FVector::Dist(P,Q);int Bars=FMath::CeilToInt(Span/17);
        for(int J=1;J<Bars;++J){FVector K=FMath::Lerp(P,Q,J/float(Bars));Box(M,TEXT("EX_Black"),K+FVector(-1.3f,-1.3f,217),K+FVector(1.3f,1.3f,326));}
    }
}
float Bank(float X,float Y,bool East)
{
    float T=FMath::Clamp((FMath::Abs(X)-1530.f)/1300.f,0.f,1.f);
    float Edge=FMath::Clamp((Y-1770)/350.f,0.f,1.f)*FMath::Clamp((4300-Y)/450.f,0.f,1.f);
    float Outer=1.f-FMath::SmoothStep(2830.f,3450.f,FMath::Abs(X));
    float Z=HouseCourt::Floor-3+(East?270.f:200.f)*FMath::SmoothStep(0.f,1.f,T)*Edge*Outer;
    if(!East)
    {
        // A level patch under the AC pad, with a soft join back to the hillside.
        float Outside=FMath::Max(FMath::Abs(X+2230)-170.f,FMath::Abs(Y-2660)-95.f);
        float Level=HouseCourt::Floor-3+200.f*FMath::SmoothStep(0.f,1.f,700.f/1300.f);
        Z=FMath::Lerp(Z,Level,1.f-FMath::SmoothStep(0.f,60.f,Outside));
    }
    return Z;
}
void Lawn(FHallMesh& M,FHallMesh& D,bool East)
{
    const float Sign=East?1:-1;
    for(int I=0;I<24;++I)for(int J=0;J<30;++J)
    {
        auto P=[&](int X,int Y){float WX=Sign*(1530+X*80.f),WY=1770+Y*84.f;return FVector(WX,WY,Bank(WX,WY,East)+.8f);};
        FVector A=P(I,J),B=P(I+1,J),C=P(I+1,J+1),E=P(I,J+1),N=FVector::CrossProduct(B-A,E-A).GetSafeNormal();if(N.Z<0)N=-N;
        M.Quad(TEXT("EX_Grass"),A,B,C,E,N,GrassUV);
    }
    FRandomStream R(East?761:863);
    for(int I=0;I<650;++I)
    {
        float X=Sign*R.FRandRange(1545,2960),Y=R.FRandRange(1850,4100);FVector P(X,Y,Bank(X,Y,East)+1.f);float H=R.FRandRange(3,13);
        for(int J=0;J<3;++J)
        {
            FVector O(R.FRandRange(-4,4),R.FRandRange(-4,4),0),U(R.FRandRange(-2,2),R.FRandRange(-2,2),H),W(1.1f,0,0);
            D.Quad(I%3==0?TEXT("EX_CourtDryGrass"):TEXT("EX_Leaf"),P+O-W,P+O+W,P+O+U,P+O+U,FVector::ForwardVector,UV);
        }
    }
}
void AC(FHallMesh& M,FHallMesh& D)
{
    float X=-2230,Y=2660,Z=Bank(X,Y,false)+6;
    auto V=[&](float A,float B,float C){return FVector(X+A,Y+B,Z+C);};
    Box(M,TEXT("EX_Concrete"),V(-160,-82,-8),V(160,82,6),FHallUV::World(240));
    for(float A:{-122.f,122.f})Box(M,TEXT("EX_Metal"),V(A-5,-62,6),V(A+5,62,35));
    Box(M,TEXT("EX_CourtAC"),V(-148,-70,35),V(148,70,173));
    Box(M,TEXT("EX_Grey"),V(-139,-71,42),V(139,-72,145));
    for(float B:{-71.f,71.f})for(int I=0;I<31;++I)Box(D,TEXT("EX_CourtAC"),V(-140,B-1,43+I*3.2f),V(140,B+1,44+I*3.2f));
    for(float A:{-145.f,0.f,145.f})Box(D,TEXT("EX_Metal"),V(A-1,-71,36),V(A+1,-73,170));
    for(float A:{-76.f,76.f})
    {
        Tube(M,TEXT("EX_Black"),V(A,0,172),V(A,0,175),58,48);
        Tube(D,TEXT("EX_Grey"),V(A,0,175),V(A,0,179),10,16);
        for(int I=0;I<5;++I){float T=2*PI*I/5;FVector C=V(A,0,175.5f);D.Box(TEXT("EX_Grey"),FTransform(FRotator(0,FMath::RadiansToDegrees(T)+25,0),C+FVector(28*FMath::Cos(T),28*FMath::Sin(T),0)),FVector(29,8,1),UV);}
        for(float R:{15.f,26.f,37.f,48.f,58.f})Ring(D,TEXT("EX_Metal"),V(A,0,181),R,.6f,40);
        for(int I=0;I<12;++I){float T=2*PI*I/12;Tube(D,TEXT("EX_Metal"),V(A,0,182),V(A+58*FMath::Cos(T),58*FMath::Sin(T),182),.6f,5);}
    }
    Box(D,TEXT("EX_Beige"),V(42,-74,115),V(76,-75,126));
    for(float A:{-137.f,137.f})for(float C:{49.f,154.f})Tube(D,TEXT("EX_Metal"),V(A,-73,C),V(A,-75,C),1.4f,8);
    Tube(M,TEXT("EX_Rubber"),V(-125,69,53),V(-210,140,30),3.5f);Tube(M,TEXT("EX_Metal"),V(-114,69,60),V(-203,138,34),1.5f);
}
}

void BuildHouseBasketballCourt(FHallMesh& Solid,FHallMesh& Decor,UHouseBlackHallSubsystem* L)
{
    PaintCourt(L);float F=HouseCourt::Floor;
    FHallUV CourtUV;CourtUV.Mode=2;CourtUV.Frame=FTransform(FVector(0,HouseCourt::MidY,F-20));CourtUV.BoxSize=FVector(2760,1840,40);
    Box(Solid,TEXT("EX_Court"),FVector(-1380,1900,F-40),FVector(1380,3740,F),CourtUV);
    for(int I=0;I<2;++I)Hoop(Solid,Decor,I);
    Fence(Solid,FVector(-1490,3820,F),FVector(1490,3820,F));
    Fence(Solid,FVector(-1490,1950,F),FVector(-1490,3820,F));
    Fence(Solid,FVector(1490,1950,F),FVector(1490,3820,F));
    // The south edge stays open to the existing porch arcade and basement doors.
    Lawn(Solid,Decor,true);Lawn(Solid,Decor,false);AC(Solid,Decor);
    for(float X:{-720.f,720.f})
    {
        FVector C(X,HouseCourt::MidY+280,F+.6f);Tube(Decor,TEXT("EX_Grey"),C,C+FVector(0,0,.4f),22,40);
        for(int I=-3;I<=3;++I){float Y=I*5;float W=FMath::Sqrt(21*21-Y*Y);Box(Decor,TEXT("EX_Black"),C+FVector(-W,Y-1,.5f),C+FVector(W,Y+1,.8f));}
    }
}
