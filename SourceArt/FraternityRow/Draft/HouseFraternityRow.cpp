#include "HouseFraternityRow.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallLook.h"
#include "HouseSurroundingsPolish.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseBlackHall.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace HouseFraternityRow
{
namespace
{
constexpr float Ground = -60.f;
const FName Brick(TEXT("EX_Brick")), Trim(TEXT("EX_Trim")), Stone(TEXT("EX_Stone"));
const FName Dark(TEXT("EX_Dark")), Glass(TEXT("EX_Glass")), Metal(TEXT("EX_Metal"));
const FName Wood(TEXT("EX_Wood")), Roof(TEXT("EX_Shingle")), Glow(TEXT("EX_Window"));
const FHallUV UV = FHallUV::World(100.f);

void Box(FHallMesh& M, FName K, FVector A, FVector B) { M.BoxMinMax(K, A, B, UV); }
void PerimeterBand(FHallMesh& M,FName K,float L,float R,float F,float B,float Z,float Overhang,float CornerRadius=0)
{
    // Exterior trim must stay outside the rooms and the open stairwell.
    const float FrontStart=CornerRadius>0?L+CornerRadius:L-Overhang;
    const float WestStart=CornerRadius>0?F+CornerRadius:F+1;
    Box(M,K,{FrontStart,F-Overhang,Z},{R+Overhang,F+1,Z+15});
    Box(M,K,{L-Overhang,B-1,Z},{R+Overhang,B+Overhang,Z+15});
    Box(M,K,{L-Overhang,WestStart,Z},{L+1,B-1,Z+15});
    Box(M,K,{R-1,F+1,Z},{R+Overhang,B-1,Z+15});
    if(CornerRadius<=0)return;
    const FVector Center(L+CornerRadius,F+CornerRadius,Z),Up(0,0,15);
    const float Outer=CornerRadius+Overhang,Inner=CornerRadius-1;
    for(int32 I=0;I<4;++I)
    {
        const float A=PI+I*PI/8,Beta=PI+(I+1)*PI/8;
        const FVector PA=Center+FVector(FMath::Cos(A)*Outer,FMath::Sin(A)*Outer,0);
        const FVector PB=Center+FVector(FMath::Cos(Beta)*Outer,FMath::Sin(Beta)*Outer,0);
        const FVector QA=Center+FVector(FMath::Cos(A)*Inner,FMath::Sin(A)*Inner,0);
        const FVector QB=Center+FVector(FMath::Cos(Beta)*Inner,FMath::Sin(Beta)*Inner,0);
        const FVector Normal(FMath::Cos((A+Beta)*.5f),FMath::Sin((A+Beta)*.5f),0);
        M.Quad(K,PA,PB,PB+Up,PA+Up,Normal,UV);
        M.Quad(K,QB,QA,QA+Up,QB+Up,-Normal,UV);
        M.Quad(K,PA+Up,PB+Up,QB+Up,QA+Up,FVector::UpVector,UV);
        M.Quad(K,QA,QB,PB,PA,-FVector::UpVector,UV);
    }
}
void Triangle(FHallMesh& M, FName K, FVector A, FVector B, FVector C, FVector N)
{
    auto& S = M.Sec(K); const int Base = S.V.Num();
    for (const FVector P : {A, B, C})
    { S.V.Add(P); S.N.Add(N); S.UV.Add(FVector2D(P.X / 100.f, P.Z / 100.f)); S.T.Add(FProcMeshTangent(FVector::ForwardVector, false)); }
    if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), N) < 0) S.I.Append({Base, Base + 1, Base + 2});
    else S.I.Append({Base, Base + 2, Base + 1});
}
void Window(FHallMesh& M, FVector P, FVector Right, FVector Out, bool Lit, bool Shutters)
{
    auto Panel = [&](FName K, float W, float H, float Offset)
    { const FVector C = P + Out * Offset; M.Quad(K, C - Right * W, C + Right * W, C + Right * W + FVector(0,0,H), C - Right * W + FVector(0,0,H), Out, UV); };
    if(Enabled())
    {
        auto Strip=[&](float X0,float X1,float Z0,float Z1){const FVector C=P+Out*6;M.Quad(Trim,C+Right*X0+FVector(0,0,Z0),C+Right*X1+FVector(0,0,Z0),C+Right*X1+FVector(0,0,Z1),C+Right*X0+FVector(0,0,Z1),Out,UV);};
        Strip(-93,-79,-8,200);Strip(79,93,-8,200);Strip(-79,79,-8,0);Strip(-79,79,182,200);
        Panel(Lit&&P.Z>350?Glow:FName(TEXT("EX_RowPane")),79,182,5);
        if(Lit&&P.Z<=350){const FVector C=P-Out*33;M.Quad(Glow,C-Right*68,C+Right*68,C+Right*68+FVector(0,0,176),C-Right*68+FVector(0,0,176),Out,UV);}
    }
    else if(HouseSurroundings::Enabled())HouseSurroundings::Window(M,P,Right,Out,166,190,Lit);
    else{Panel(Trim, 93, 200, 3); Panel(Lit ? Glow : Glass, 79, 182, 5);}
    const FVector C = P + Out * 7;
    M.Quad(Trim, C-Right*3, C+Right*3, C+Right*3+FVector(0,0,182), C-Right*3+FVector(0,0,182), Out, UV);
    M.Quad(Trim, C-Right*79+FVector(0,0,88), C+Right*79+FVector(0,0,88), C+Right*79+FVector(0,0,94), C-Right*79+FVector(0,0,94), Out, UV);
    if (Shutters) for (float Side : {-1.f, 1.f})
    {
        const FVector S = C + Right * Side * 124.f;
        M.Quad(TEXT("EX_FratShutter"), S-Right*23, S+Right*23, S+Right*23+FVector(0,0,193), S-Right*23+FVector(0,0,193), Out, UV);
        for (int I=0; I<10; ++I) { const FVector Q = S + Out * 2 + FVector(0,0,10+I*18);
            M.Quad(Dark, Q-Right*21, Q+Right*21, Q+Right*21+FVector(0,0,2), Q-Right*21+FVector(0,0,2), Out, UV); }
    }
}
void LegacyLetters(FHallMesh& M, FVector C, int Style)
{
    // Architectural Greek-letter strokes, without font-atlas dependencies.
    if (Style==2) C.X-=52.5f;
    auto Line = [&](int Letter, FVector2D A, FVector2D B)
    { const float X = C.X - (Letter-1)*105.f; M.Cylinder(TEXT("EX_FratLetter"), FVector(X-A.X,C.Y,C.Z+A.Y), FVector(X-B.X,C.Y,C.Z+B.Y), 5.f, 6, UV); };
    auto Sigma = [&](int I) { Line(I,{-31,68},{31,68});Line(I,{-31,68},{5,34});Line(I,{5,34},{-31,0});Line(I,{-31,0},{31,0}); };
    auto Delta = [&](int I) { Line(I,{-34,0},{0,68});Line(I,{0,68},{34,0});Line(I,{34,0},{-34,0}); };
    auto Tau = [&](int I) { Line(I,{-32,68},{32,68});Line(I,{0,68},{0,0}); };
    if (Style==0) { Sigma(0);Line(1,{-32,0},{0,68});Line(1,{0,68},{32,0});Line(1,{-18,28},{18,28});Line(2,{-28,0},{-28,68});Line(2,{-28,68},{30,68});Line(2,{-28,34},{22,34});Line(2,{-28,0},{30,0}); }
    else if (Style==1) { Delta(0); Tau(1); Delta(2); }
    else { Line(0,{-28,0},{-28,68});Line(0,{-28,34},{29,68});Line(0,{-28,34},{29,0});Sigma(1); }
}

void Word(FHallMesh& M,FVector C,int32 HouseIndex)
{
    // Original four-letter fictional combinations. 0Omega,1Xi,2Lambda,3Eta,4Psi,5Rho.
    const int32 Words[5][4]={{0,1,2,3},{4,3,1,5},{3,1,0,5},{2,0,3,1},{5,1,3,4}};
    for(int32 I=0;I<4;++I)
    {
        auto Stroke=[&](float A,float B,float D,float E){M.Cylinder(TEXT("EX_FratLetter"),C+FVector(-((I-1.5f)*100.f+A),0,B),C+FVector(-((I-1.5f)*100.f+D),0,E),4,6,UV);};
        switch(Words[HouseIndex][I])
        {
        case 0:
            for(int K=0;K<18;++K){float A=FMath::DegreesToRadians(-35.f+K*250.f/18.f),B=FMath::DegreesToRadians(-35.f+(K+1)*250.f/18.f);Stroke(FMath::Cos(A)*29,32+FMath::Sin(A)*30,FMath::Cos(B)*29,32+FMath::Sin(B)*30);}
            Stroke(-34,0,-10,0);Stroke(10,0,34,0);break;
        case 1:Stroke(-30,64,30,64);Stroke(-23,32,23,32);Stroke(-30,0,30,0);break;
        case 2:Stroke(-30,0,0,64);Stroke(0,64,30,0);break;
        case 3:Stroke(-27,0,-27,64);Stroke(27,0,27,64);Stroke(-27,32,27,32);break;
        case 4:Stroke(0,0,0,64);Stroke(-28,64,-28,35);Stroke(-28,35,0,22);Stroke(0,22,28,35);Stroke(28,35,28,64);break;
        default:Stroke(-26,0,-26,64);Stroke(-26,64,20,64);Stroke(20,64,30,50);Stroke(30,50,20,34);Stroke(20,34,-26,34);break;
        }
    }
}
void Letters(FHallMesh& M,FVector C,int Style)
{
    if(!Enabled()){LegacyLetters(M,C,Style);return;}
    Word(M,C,Style==2?0:Style==1?1:2);
}

void House(FHallMesh& M, float X, float Y, float W, float D, float H, int Style,FHallMesh* Structure=nullptr)
{
    FHallMesh& S=Structure?*Structure:M;
    const float L=X-W*.5f,R=X+W*.5f,F=Y-D*.5f,B=Y+D*.5f,Top=Ground+H;
    const FName Body = Style==1 ? FName(TEXT("EX_FratSiding")) : Style==2 ? FName(TEXT("EX_BrickRed")) : Brick;
    if(!Enabled()) { Box(M,Stone,{L-12,F-12,Ground-45},{R+12,B+12,Ground+90});
    Box(M,Body,{L,F,Ground+80},{R,B,Top}); }
    for (float Z : {Ground+460,Top-18})
    {
        if(Enabled())PerimeterBand(M,Trim,L,R,F,B,Z,14);
        else Box(M,Trim,{L-14,F-14,Z},{R+14,B+14,Z+15});
    }
    if (Style==1 && !Enabled()) for (float Z=Ground+90; Z<Top; Z+=25)
    { Box(M,Trim,{L-1,F-3,Z},{R+1,F,Z+2});Box(M,Trim,{L-3,F,Z},{L,B,Z+2});Box(M,Trim,{R,F,Z},{R+3,B,Z+2}); }
    for (float XX : {L-10,R-10}) for (float YY : {F-10,B-10})
        Box(M,Trim,{XX,YY,Ground+90},{XX+22,YY+22,Top});
    for (int Floor=0;Floor<2;++Floor)
    {
        const float Z=Ground+135+Floor*390.f;
        for (int I=0;I<5;++I) { const float XX=FMath::Lerp(L+230,R-230,I/4.f);
            if (Floor==0 && I==2) continue;
            Window(M,{XX,F-5,Z},{1,0,0},{0,-1,0},(Enabled()?(Style==2?Floor==1&&I==2:Style==1?Floor==0&&I==1:Floor==0):(I+Floor+Style)%5==1),true);
            Window(M,{XX,B+5,Z},{-1,0,0},{0,1,0},false,false); }
        for (float XX : {L-5,R+5}) for (int I=0;I<4;++I)
            Window(M,{XX,F+280+I*(D-560)/3,Z},{0,1,0},{XX<X?-1.f:1.f,0,0},(Enabled()?Style==0&&Floor==0:(I+Floor+Style)%6==1),Style!=1);
    }
    // Closed pitched roof, pale fascia and two substantial brick chimneys.
    const float Eave=Top+35,Ridge=Top+350;
    const FName RoofMat=HouseSurroundings::Enabled()?FName(TEXT("EX_FratRoof")):Roof;
    S.Quad(RoofMat,{L-85,F-90,Eave},{X,F-90,Ridge},{X,B+90,Ridge},{L-85,B+90,Eave},{-.25f,0,.97f},FHallUV::World(120));
    S.Quad(RoofMat,{X,F-90,Ridge},{R+85,F-90,Eave},{R+85,B+90,Eave},{X,B+90,Ridge},{.25f,0,.97f},FHallUV::World(120));
    for (float YY : {F-20,B+20}) Triangle(S,Trim,{L-60,YY,Top},{R+60,YY,Top},{X,YY,Ridge-10},{0,YY<Y?-1.f:1.f,0});
    Box(S,Trim,{L-90,F-90,Top+12},{L-65,B+90,Eave});Box(S,Trim,{R+65,F-90,Top+12},{R+90,B+90,Eave});
    for (float XX : {X-W*.32f,X+W*.32f})
    { Box(S,Brick,{XX-75,Y+220,Top},{XX+75,Y+420,Ridge+130});Box(S,Stone,{XX-85,Y+210,Ridge+112},{XX+85,Y+430,Ridge+145});Box(S,Dark,{XX-56,Y+245,Ridge+145},{XX+56,Y+395,Ridge+146}); }
    // Tall colonnade and second-floor balcony establish the fraternity-house silhouette.
    const float PL=X-W*.35f,PR=X+W*.35f,PF=F-560,Deck=Ground+90,PorchTop=Top-95;
    if(!Enabled()) { Box(S,Stone,{PL-55,PF-75,Ground-25},{PR+55,F+10,Deck});
    for (int Step=0;Step<5;++Step) Box(S,Stone,{X-205,PF-290+Step*43,Ground-10},{X+205,PF-242+Step*43,Ground+10+Step*16}); }
    for (int I=0;I<4;++I)
    {
        const float XX=FMath::Lerp(PL,PR,I/3.f);
        Box(S,Trim,{XX-58,PF-58,Deck},{XX+58,PF+58,Deck+28});
        S.Cylinder(Trim,{XX,PF,Deck+28},{XX,PF,PorchTop-26},42,14,UV);
        S.Cylinder(Trim,{XX,PF,PorchTop-26},{XX,PF,PorchTop},56,14,UV);
    }
    Box(S,Trim,{PL-70,PF-68,PorchTop},{PR+70,F+50,PorchTop+96});
    S.Quad(RoofMat,{PL-90,PF-85,PorchTop+98},{X,PF-85,PorchTop+310},{X,F+80,PorchTop+310},{PL-90,F+80,PorchTop+98},{-.25f,0,.97f},UV);
    S.Quad(RoofMat,{X,PF-85,PorchTop+310},{PR+90,PF-85,PorchTop+98},{PR+90,F+80,PorchTop+98},{X,F+80,PorchTop+310},{.25f,0,.97f},UV);
    Triangle(S,Trim,{PL-80,PF-87,PorchTop+96},{PR+80,PF-87,PorchTop+96},{X,PF-87,PorchTop+300},{0,-1,0});
    Box(S,Dark,{X-225,PF-92,PorchTop+112},{X+225,PF-89,PorchTop+211});Letters(M,{X,PF-96,PorchTop+126},Style);
    const float Balcony=Ground+480;
    Box(S,Trim,{PL,PF+80,Balcony-20},{PR,F+15,Balcony});
    Box(S,Trim,{PL,PF+85,Balcony+88},{PR,PF+93,Balcony+96});
    for(float XX=PL+15;XX<PR;XX+=52)Box(S,Trim,{XX,PF+85,Balcony},{XX+5,PF+91,Balcony+90});
    // Front doors, lantern housings, benches, path and lawn edging are scenery.
    if(!Enabled()) {
    Box(M,Trim,{X-117,F-13,Deck},{X+117,F-4,Deck+300});Box(M,Wood,{X-103,F-16,Deck},{X+103,F-13,Deck+286});
    for(float XX:{X-98,X+5})for(float Z:{Deck+28,Deck+153})Box(M,Dark,{XX+10,F-18,Z},{XX+85,F-16,Z+97});
    for(float XX:{X-13,X+13})Box(M,Metal,{XX-3,F-23,Deck+110},{XX+3,F-18,Deck+145});
    }
    for(float XX:{X-200,X+200}) {Box(M,Metal,{XX-19,F-45,Deck+167},{XX+19,F-12,Deck+226});Box(M,Enabled()&&Style==2?Dark:Glow,{XX-13,F-46,Deck+174},{XX+13,F-44,Deck+219});}
    Box(M,TEXT("EX_Concrete"),{X-150,-3900,Ground+1},{X+150,PF-245,Ground+7});
    for(float XX:{PL-90,PR+90}) {Box(M,Stone,{XX-55,PF-30,Deck},{XX+55,PF+80,Deck+64});if(!HouseSurroundings::Enabled())Box(M,TEXT("EX_Hedge"),{XX-47,PF-22,Deck+61},{XX+47,PF+70,Deck+115});}
    for(float XX:{Enabled()?PL+180:L+235,Enabled()?PR-180:R-235}) {Box(S,Wood,{XX-105,F-365,Deck+35},{XX+105,F-315,Deck+42});Box(S,Wood,{XX-105,F-320,Deck+42},{XX+105,F-310,Deck+82});for(float Q:{XX-85,XX+85})Box(S,Metal,{Q-3,F-357,Deck},{Q+3,F-320,Deck+35});}
    // A pair of pendant banners beneath the balcony, with broad colour bands.
    for(float XX:{PL+180,PR-180}) {Box(M,TEXT("EX_FratBanner"),{XX-45,F-38,Deck+160},{XX+45,F-33,Deck+375});Box(M,Trim,{XX-45,F-40,Deck+214},{XX+45,F-38,Deck+230});}
    if(HouseSurroundings::Enabled())
    {
        auto Beam=[&](FName K,FVector A,FVector Bp,float Width,float Depth){HouseSurroundings::Beam(M,K,A,Bp,Width,Depth);};
        // Moulded capitals, layered bases, soffits, balcony returns and rainwater goods.
        for(int I=0;I<4;++I){const float XX=FMath::Lerp(PL,PR,I/3.f);
            for(float Z:{Deck+30,Deck+45,PorchTop-49,PorchTop-33})M.Cylinder(Trim,{XX,PF,Z},{XX,PF,Z+9},Z<Deck+90?49:52,18,UV);
            Box(M,Trim,{XX-60,PF-60,PorchTop-4},{XX+60,PF+60,PorchTop+11});}
        for(float XX:{PL,PR}){Beam(Trim,{XX,PF+90,Balcony+92},{XX,F-5,Balcony+92},7,9);
            for(float YY=PF+115;YY<F-10;YY+=52)Box(M,Trim,{XX-3,YY,Balcony},{XX+3,YY+5,Balcony+91});}
        for(float XX=PL;XX<PR;XX+=62)Box(M,Trim,{XX,PF-70,PorchTop-9},{XX+12,PF+18,PorchTop+4});
        for(float XX:{L-78,R+78}){Box(M,Metal,{XX-10,F-87,Eave-18},{XX+10,B+87,Eave-4});
            for(float YY:{F-60,B+60}){M.Cylinder(Trim,{XX,YY,Ground+5},{XX,YY,Eave-9},5,10,UV);M.Cylinder(Trim,{XX,YY,Ground+5},{XX,YY-22,Ground-5},5,10,UV);}}
        Beam(Trim,{L-70,F-26,Top+12},{X,F-26,Ridge-17},14,18);Beam(Trim,{X,F-26,Ridge-17},{R+70,F-26,Top+12},14,18);
        for(float XX:{X-W*.32f,X+W*.32f}){Box(M,Metal,{XX-93,Y+203,Ridge+146},{XX+93,Y+438,Ridge+155});
            for(float ZZ=Top+36;ZZ<Ridge+110;ZZ+=47)Box(M,Dark,{XX-75,Y+218,ZZ},{XX+75,Y+222,ZZ+2});}
        // Bay windows and window-box flowers distinguish the three facades.
        for(float XX:{L+270,R-270})
        {
            Box(M,TEXT("EX_DetailStone"),{XX-127,F-90,Ground+96},{XX+127,F-12,Ground+134});
            Box(M,Body,{XX-112,F-78,Ground+129},{XX+112,F-2,Ground+335});
            HouseSurroundings::Window(M,{XX,F-81,Ground+139},{1,0,0},{0,-1,0},195,182,false);
            Box(M,Trim,{XX-137,F-105,Ground+331},{XX+137,F+2,Ground+348});
            Box(M,TEXT("EX_PotClay"),{XX-98,F-123,Ground+110},{XX+98,F-86,Ground+135});
            Box(M,TEXT("EX_PotSoil"),{XX-92,F-117,Ground+135},{XX+92,F-92,Ground+137});
            for(int J=0;J<9;++J){const FVector P(XX-84+J*21,F-106,Ground+137);M.Cylinder(TEXT("EX_Hedge"),P,P+FVector(0,0,28+J%3*5),1,5,UV);M.Dome(J%3==0?FName(TEXT("EX_FlowerPetal")):FName(TEXT("EX_Orange")),P+FVector(0,0,30+J%3*5),FVector::UpVector,5,5,7);}
        }
        Box(M,Dark,{X-91,F-80,Deck+3},{X+91,F-22,Deck+4});
        HouseSurroundings::Window(M,{X,F-24,Deck+296},{1,0,0},{0,-1,0},200,58,false);
        if(!Enabled()) {
        for(float XX:{X-90,X+14})for(float Z:{Deck+22,Deck+150})
        {Beam(Wood,{XX,F-22,Z},{XX+76,F-22,Z},6,5);Beam(Wood,{XX,F-22,Z+104},{XX+76,F-22,Z+104},6,5);for(float Q:{XX,XX+76})Beam(Wood,{Q,F-22,Z},{Q,F-22,Z+104},6,5);}
        }
        if(Style==1)HouseSurroundings::Window(M,{X,F-30,Top+100},{1,0,0},{0,-1,0},125,110,false);
        else{M.Cylinder(Trim,{X,F-27,Top+174},{X,F-35,Top+174},59,24,UV);M.Cylinder(Dark,{X,F-35,Top+174},{X,F-37,Top+174},46,24,UV);Beam(Trim,{X-45,F-40,Top+174},{X+45,F-40,Top+174},5,5);Beam(Trim,{X,F-40,Top+129},{X,F-40,Top+219},5,5);}
    }
}
}

bool Enabled(){return !FParse::Param(FCommandLine::Get(),TEXT("NoFraternityRowUpgrade"));}
bool LightsEnabled(){return Enabled()&&!FParse::Param(FCommandLine::Get(),TEXT("NoFraternityRowLights"));}
int32 ExteriorLightBudget(){return Enabled()?(LightsEnabled()?57:51):53;}
const TArray<FHouse>& Houses()
{
    static const TArray<FHouse> H={
        {TEXT("R1"),0,-18625,-16175,-2545,-295,30,860},
        {TEXT("R2"),1,-15300,-12500,-2530,-430,30,730},
        {TEXT("R3"),2,-11350,-8850,-2600,-400,30,810},
        {TEXT("R4"),3,-8400,-6800,450,1850,-50,660},
        {TEXT("R5"),4,-7080,-5680,-1700,-200,30,740}};
    return H;
}

float RearStairX(const FHouse& H){return H.X()+(H.Index==4?80.f:0.f);}
FVector PorchLight(const FHouse& H)
{ return {H.X()-230,H.Index==2?H.F-450:H.Index==4?H.F-90:210,H.Floor+220}; }
FVector YardLight(const FHouse& H)
{
    const FVector2D A[]={{-11600,240},{-8280,2370},{-6090,2370}};
    const FVector2D B[]={{-10630,400},{-7850,3170},{-5660,2450}};
    const int32 I=FMath::Clamp(H.Index-2,0,2);const float T=I==0?8.f/18.f:.5f;
    const FVector2D P=FMath::Lerp(A[I],B[I],T);
    const float Top=FMath::Max(HouseExteriorSite::GroundHeight(A[I].X,A[I].Y),HouseExteriorSite::GroundHeight(B[I].X,B[I].Y))+275;
    return FVector(P,Top-42*FMath::Sin(PI*T)-8);
}

void CreateLook(UHouseBlackHallSubsystem* Look)
{
    HallMakePlain(Look,TEXT("EX_FratSiding"),FLinearColor(.59f,.56f,.47f),.85f);
    HallMakePlain(Look,TEXT("EX_FratShutter"),FLinearColor(.028f,.054f,.043f),.78f);
    HallMakePlain(Look,TEXT("EX_FratLetter"),FLinearColor(.73f,.57f,.22f),.55f,.2f,FLinearColor(.04f,.027f,.009f));
    HallMakePlain(Look,TEXT("EX_FratBanner"),FLinearColor(.21f,.025f,.035f),.96f);
    if(!Enabled())return;
    auto Finish=[&](FName Key,FLinearColor Color,int Seed,bool Siding)
    {
        FHallImage Img;Img.Init(256,256,Color);
        for(int Y=0;Y<256;++Y)for(int X=0;X<256;++X)
        {
            const float N=(HallNoise(X*.12f,Y*.11f,Seed)-.5f)*.12f+(HallFbm(X*.025f,Y*.025f,3,Seed+1)-.5f)*.15f;
            const float Joint=Siding&&(Y%32<2)?-.18f:0.f;
            Img.At(X,Y)=FLinearColor(FMath::Max(.01f,Color.R+N+Joint),FMath::Max(.01f,Color.G+N*.9f+Joint),FMath::Max(.01f,Color.B+N*.8f+Joint));
        }
        auto* T=HallMakeTexture(Img,true,false,*FString::Printf(TEXT("T_%s"),*Key.ToString()));Look->Textures.Add(Key,T);HallMakeTextured(Look,Key,T,nullptr,.87f);
    };
    Finish(TEXT("EX_FratSiding"),FLinearColor(.59f,.56f,.47f),8609,true);
    Finish(TEXT("EX_RowPlaster"),FLinearColor(.55f,.48f,.34f),8611,false);
    Finish(TEXT("EX_RowBlueSiding"),FLinearColor(.23f,.31f,.34f),8613,true);
    Finish(TEXT("EX_RowCream"),FLinearColor(.67f,.61f,.45f),8615,false);
    HallMakePlain(Look,TEXT("EX_RowTimber"),FLinearColor(.075f,.049f,.027f),.86f);
    HallMakePlain(Look,TEXT("EX_RowCurtain"),FLinearColor(.24f,.12f,.075f),.97f);
    if(auto* Parent=HallHouseMat(TEXT("M_House_Glass")))
    {
        auto* G=UMaterialInstanceDynamic::Create(Parent,Look);G->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.03f,.04f,.047f));
        G->SetScalarParameterValue(TEXT("Opacity"),.24f);G->SetScalarParameterValue(TEXT("Roughness"),.12f);Look->Materials.Add(TEXT("EX_RowPane"),G);
    }
    else HallMakePlain(Look,TEXT("EX_RowPane"),FLinearColor(.025f,.034f,.04f),.13f,.1f);
    for(int I=0;I<5;++I)
    {
        FHallImage N;N.Init(256,128,FLinearColor(.07f,.06f,.04f));FHallTextStyle S;S.Height=70;S.Stroke=6;
        const FString Text=FString::FromInt(177+I*2);HallDrawText(N,Text,{(256-HallTextWidth(Text,S))*.5f,97},S,FLinearColor(.8f,.72f,.52f));
        const FName K(*FString::Printf(TEXT("EX_RowNumber%d"),I));auto* T=HallMakeTexture(N,true,false,*K.ToString());Look->Textures.Add(K,T);HallMakeTextured(Look,K,T,nullptr,.85f);
    }

}
TArray<FBox2D> Build(FHallMesh& M)
{
    FHallMesh Structure;
    House(M,-10100,-1500,2500,2200,900,0,Enabled()?&Structure:nullptr);
    House(M,-13900,-1480,2800,2100,820,1,Enabled()?&Structure:nullptr);
    House(M,-17400,-1420,2450,2250,950,2,Enabled()?&Structure:nullptr);
    return {FBox2D({-11600,-3720},{-8600,100}),FBox2D({-15550,-3700},{-12250,100}),FBox2D({-18850,-3750},{-15950,100})};
}
}

namespace HouseFraternityRow
{
namespace
{
struct FWallHole {int32 Side;float U0,U1,Z0,Z1;};
void GroundWindows(const FHouse& H,TArray<FWallHole>& Out)
{
    const bool Old=H.Index<3;const float Sill=H.Floor+(Old?45.f:95.f);const int N=Old?5:3;
    for(int I=0;I<N;++I)if(I!=N/2)
    {
        float X=FMath::Lerp(H.L+(Old?230.f:200.f),H.R-(Old?230.f:200.f),float(I)/(N-1));
        if(H.Index==4&&I==0)X=H.L+280;
        Out.Add({0,X-79,X+79,Sill,Sill+182});Out.Add({1,X-79,X+79,Sill,Sill+182});
    }
    const int NS=Old?4:3;
    for(int I=0;I<NS;++I)
    {
        const float Y=FMath::Lerp(H.F+(Old?280.f:250.f),H.B-(Old?280.f:250.f),float(I)/(NS-1));
        Out.Add({2,Y-79,Y+79,Sill,Sill+182});Out.Add({3,Y-79,Y+79,Sill,Sill+182});
    }
}
void WallPiece(FHallMesh& M,const FHouse& H,FName K,int S,float U0,float U1,float Z0,float Z1,float D,float Inset)
{
    if(U1-U0<.01f||Z1-Z0<.01f)return;
    if(S==0)Box(M,K,{U0,H.F+Inset,Z0},{U1,H.F+Inset+D,Z1});
    else if(S==1)Box(M,K,{U0,H.B-Inset-D,Z0},{U1,H.B-Inset,Z1});
    else if(S==2)Box(M,K,{H.L+Inset,U0,Z0},{H.L+Inset+D,U1,Z1});
    else Box(M,K,{H.R-Inset-D,U0,Z0},{H.R-Inset,U1,Z1});
}
void Face(FHallMesh& M,FName K,FVector C,FVector Along,FVector Out,float W,float H)
{M.QuadUV(K,C-Along*W*.5f,C+Along*W*.5f,C+Along*W*.5f+FVector(0,0,H),C-Along*W*.5f+FVector(0,0,H),Out,{0,1},{1,1},{1,0},{0,0},Along);}
void Beam(FHallMesh& M,FName K,FVector A,FVector B,float W,float D)
{HouseSurroundings::Beam(M,K,A,B,W,D);}
void DoorFrame(FHallMesh& M,const FHouse& H,bool Rear)
{
    const float Y=Rear?H.B+1:H.F-1,W=Rear?140:200,Z=H.Floor;
    for(float X:{H.X()-W*.5f-9,H.X()+W*.5f+9})Box(M,Trim,{X-9,Y-10,Z},{X+9,Y+10,Z+242});
    Box(M,Trim,{H.X()-W*.5f-18,Y-11,Z+228},{H.X()+W*.5f+18,Y+11,Z+245});
    if(!Rear)Face(M,*FString::Printf(TEXT("EX_RowNumber%d"),H.Index),{H.X()+170,Y-13,Z+147},{-1,0,0},{0,-1,0},74,37);
}
void Bicycle(FHallMesh& S,FHallMesh& D,const FVector& Base)
{
    auto V=[&](float X,float Y,float Z){return Base+FVector(X,Y,Z);};
    for(float X:{-65.f,65.f})
    {
        for(int32 I=0;I<24;++I)
        {
            const float A=I*2*PI/24,B=(I+1)*2*PI/24;
            S.Cylinder(Dark,V(X+34*FMath::Cos(A),0,34+34*FMath::Sin(A)),V(X+34*FMath::Cos(B),0,34+34*FMath::Sin(B)),2,6,UV);
            if(I%2==0)D.Cylinder(Metal,V(X,0,34),V(X+32*FMath::Cos(A),0,34+32*FMath::Sin(A)),.35f,5,UV);
        }
        S.Cylinder(Metal,V(X,-3,34),V(X,3,34),4,8,UV);
    }
    const FVector A=V(-65,0,34),B=V(0,0,26),C=V(-18,0,82),E=V(45,0,78),F=V(65,0,34);
    for(const auto& Edge:TArray<TPair<FVector,FVector>>{{A,B},{B,C},{C,A},{C,E},{E,B},{E,F}})
        S.Cylinder(TEXT("EX_FratShutter"),Edge.Key,Edge.Value,1.7f,7,UV);
    S.Cylinder(Metal,E,V(45,0,101),1.5f,6,UV);S.Cylinder(Metal,V(45,-23,101),V(45,23,101),1.5f,6,UV);
    Box(S,Dark,V(-31,-9,84),V(-5,9,90));
    S.Cylinder(Metal,V(0,-19,26),V(0,19,26),1.7f,6,UV);
    S.Cylinder(Metal,B,V(8,18,0),1.2f,6,UV); // Visible kickstand contacts the same porch datum.
}
float SupportedBase(float L,float R,float F,float B)
{
    float Z=MAX_flt;
    for(float X:{L,R,(L+R)*.5f})for(float Y:{F,B,(F+B)*.5f})
        Z=FMath::Min(Z,HouseExteriorSite::GroundHeight(X,Y));
    return Z-20.f;
}
void RearStair(FHallMesh& M,const FHouse& H)
{
    if(H.Index==4)
    {
        // Wide offset landing preserves the fixed common path atX-6550.
        // Its last landing meets the near bank and has a full buried foundation
        // under the descending grade, rather than sampling beyond its footprint.
        const float SX=RearStairX(H),BottomY=H.B+288,EndY=BottomY+120;
        const float Z=HouseExteriorSite::GroundHeight(SX,BottomY),Rise=(H.Floor-Z)/7.f;
        auto Founded=[&](float F,float B,float Top)
        {Box(M,Stone,{SX-200,F,SupportedBase(SX-200,SX+200,F,B)},{SX+200,B,Top});};
        Founded(H.B-3,H.B+120,H.Floor);
        for(int I=1;I<7;++I){const float Y=H.B+120+(I-1)*28;Founded(Y,Y+28,H.Floor-I*Rise);}
        Founded(BottomY,EndY,Z);
        for(float RailX:{SX-190,SX+190})
        {
            Beam(M,Metal,{RailX,H.B+8,H.Floor+90},{RailX,H.B+115,H.Floor+90},4,4);
            Beam(M,Metal,{RailX,H.B+115,H.Floor+90},{RailX,BottomY+12,Z+90},4,4);
            for(const FVector P:{FVector(RailX,H.B+10,H.Floor),FVector(RailX,H.B+110,H.Floor),FVector(RailX,BottomY+10,Z)})
                M.Cylinder(Metal,P,P+FVector(0,0,90),2.5f,8,UV);
        }
        return;
    }
    const float End=H.B+(H.Index<3?380.f:500.f),Z=HouseExteriorSite::GroundHeight(H.X(),End);
    const int N=H.Index<3?6:7;const float Rise=(H.Floor-Z)/N;
    Box(M,Stone,{H.X()-200,H.B-3,Z-18},{H.X()+200,H.B+120,H.Floor});
    for(int I=1;I<N;++I)
    {
        const float Y=H.B+120+(I-1)*28;
        Box(M,Stone,{H.X()-200,Y,Z-16},{H.X()+200,Y+28,H.Floor-I*Rise});
    }
    // Last landing follows the existing ground, never raises a fixed probe.
    const float Y=H.B+120+(N-1)*28;
    Box(M,Stone,{H.X()-200,Y,Z-14},{H.X()+200,Y+120,Z});
    for(float X:{H.X()-190,H.X()+190})
    {
        Beam(M,Metal,{X,H.B+8,H.Floor+90},{X,H.B+115,H.Floor+90},4,4);
        Beam(M,Metal,{X,H.B+115,H.Floor+90},{X,Y+12,Z+90},4,4);
        for(const FVector P:{FVector(X,H.B+10,H.Floor),FVector(X,H.B+110,H.Floor),FVector(X,Y+10,Z)})M.Cylinder(Metal,P,P+FVector(0,0,90),2.5f,8,UV);
    }
}
void PorchFloor(FHallMesh& M,const FHouse& H)
{
    if(H.Index<3)
    {
        const float PL=H.X()-H.Width()*.35f,PR=H.X()+H.Width()*.35f,PF=H.F-560;
        Box(M,Stone,{PL-55,PF-75,Ground-25},{PR+55,H.F+10,H.Floor});
        for(int S=0;S<5;++S)Box(M,Stone,{H.X()-205,PF-290+S*43,Ground-10},{H.X()+205,PF-242+S*43,Ground+10+S*16});
        return;
    }
    const float Front=H.Index==4?H.F-150.f:150.f;
    if(H.Index==4)
    {
        Box(M,Stone,{H.L,Front,SupportedBase(H.L,H.R,Front,H.F)},{H.R,H.F,H.Floor});
        Box(M,Stone,{H.L-150,Front,SupportedBase(H.L-150,H.L,Front,H.F+550)},{H.L,H.F+550,H.Floor});
        const float Base=HouseExteriorSite::GroundHeight(H.X(),Front-140),Rise=(H.Floor-Base)/6.f;
        for(int I=0;I<5;++I)
        {
            const float F=Front-140+I*28;
            Box(M,Stone,{H.X()-200,F,SupportedBase(H.X()-200,H.X()+200,F,F+28)},
                {H.X()+200,F+28,Base+(I+1)*Rise});
        }
        return;
    }
    Box(M,Stone,{H.L,Front,-130},{H.R,H.F,H.Floor});
    const float Bottom=HouseExteriorSite::GroundHeight(H.X(),122),Mid=(H.Floor+Bottom)*.5f;
    Box(M,Stone,{H.X()-200,122,Bottom-12},{H.X()+200,150,Mid});
    // The second riser is the edge of the porch itself.
}
void NewHouse(FHallMesh& M,FHallMesh& Solid,FHallMesh& GlowMesh,const FHouse& H)
{
    const float Front=H.Index==4?H.F-150.f:150.f;
    const bool Tudor=H.Index==3;const FName Body=Tudor?FName(TEXT("EX_RowPlaster")):FName(TEXT("EX_RowBlueSiding"));
    const FName Pale=Tudor?FName(TEXT("EX_RowCream")):Trim;
    const float Top=H.Floor+H.Height,Peak=Top+(Tudor?470:410),X=H.X();
    // A real soffit closes the attic underside at the existing eaves, above every usable upper landing.
    Box(Solid,Pale,{H.L-50,H.F-55,Top-8},{H.R+50,H.B+55,Top});
    if(Tudor)
    {
        // Only the cross-gable's two exposed strips extend beyond the main roof base.
        Box(Solid,Pale,{H.L-55,H.F-70,Top-8},{H.L-50,H.F+610,Top});
        Box(Solid,Pale,{H.L-50,H.F-70,Top-8},{X+70,H.F-55,Top});
    }
    // Different roof silhouettes: cross gable Tudor, hipped Victorian with a corner tower.
    if(Tudor)
    {
        Solid.Quad(TEXT("EX_FratRoof"),{H.L-50,H.F-55,Top},{X,H.F-55,Peak},{X,H.B+55,Peak},{H.L-50,H.B+55,Top},{-.5f,0,.86f},UV);
        Solid.Quad(TEXT("EX_FratRoof"),{X,H.F-55,Peak},{H.R+50,H.F-55,Top},{H.R+50,H.B+55,Top},{X,H.B+55,Peak},{.5f,0,.86f},UV);
        for(float Y:{H.F-55,H.B+55})Triangle(Solid,Body,{H.L-50,Y,Top},{H.R+50,Y,Top},{X,Y,Peak},{0,Y<H.Y()?-1.f:1.f,0});
        const float CY=H.F+260,CP=Top+355;
        Solid.Quad(TEXT("EX_FratRoof"),{H.L-55,H.F-70,Top},{X+70,H.F-70,Top},{X+70,CY,CP},{H.L-55,CY,CP},{0,-.72f,.69f},UV);
        Solid.Quad(TEXT("EX_FratRoof"),{H.L-55,CY,CP},{X+70,CY,CP},{X+70,H.F+610,Top},{H.L-55,H.F+610,Top},{0,.72f,.69f},UV);
        Triangle(Solid,Body,{H.L-55,H.F-70,Top},{H.L-55,H.F+610,Top},{H.L-55,CY,CP},{-1,0,0});
        Triangle(Solid,Body,{X+70,H.F-70,Top},{X+70,H.F+610,Top},{X+70,CY,CP},{1,0,0});
    }
    else
    {
        const FVector RF(X,H.F+600,Peak),RB(X,H.B-600,Peak);
        Solid.Quad(TEXT("EX_FratRoof"),{H.L-50,H.F-55,Top},RF,RB,{H.L-50,H.B+55,Top},{-.5f,0,.86f},UV);
        Solid.Quad(TEXT("EX_FratRoof"),RF,{H.R+50,H.F-55,Top},{H.R+50,H.B+55,Top},RB,{.5f,0,.86f},UV);
        Triangle(Solid,TEXT("EX_FratRoof"),{H.L-50,H.F-55,Top},{H.R+50,H.F-55,Top},RF,{0,-.6f,.8f});
        Triangle(Solid,TEXT("EX_FratRoof"),{H.R+50,H.B+55,Top},{H.L-50,H.B+55,Top},RB,{0,.6f,.8f});
    }
    for(float Z:{H.Floor+330,Top-20})PerimeterBand(M,Pale,H.L,H.R,H.F,H.B,Z,10,H.Index==4?150.f:0.f);
    for(float XX:{H.L,H.R})if(Tudor||XX!=H.L)Box(M,Pale,{XX-9,H.F-6,H.Floor},{XX+9,H.F+9,Top});
    if(!Tudor){Box(M,Pale,{H.L-6,H.F+145,H.Floor},{H.L+9,H.F+163,Top});Box(M,Pale,{H.L+145,H.F-6,H.Floor},{H.L+163,H.F+9,Top});}
    for(int Floor=0;Floor<2;++Floor)
    {
        const float Z=H.Floor+95+Floor*(Tudor?330.f:370.f);
        for(int I=0;I<3;++I)
        {
            const float XX=H.Index==4&&I==0?H.L+280:FMath::Lerp(H.L+200,H.R-200,I*.5f);
            if(!(Floor==0&&I==1)){Window(M,{XX,H.F-5,Z},{1,0,0},{0,-1,0},Floor==0&&!Tudor,false);Window(M,{XX,H.B+5,Z},{-1,0,0},{0,1,0},Floor==0&&Tudor,false);}
            const float YY=FMath::Lerp(H.F+250,H.B-250,I*.5f);
            for(float SX:{H.L-5,H.R+5})Window(M,{SX,YY,Z},{0,1,0},{SX<X?-1.f:1.f,0,0},Floor==0&&I==0,false);
        }
    }
    // Porch columns, brackets and low rail with a generous central entry.
    const float PorchRoof=H.Floor+280;
    for(float XX:{H.L+60,H.L+H.Width()*.28f,H.R-H.Width()*.28f,H.R-60})
    {
        Box(Solid,Pale,{XX-12,Front+20,H.Floor},{XX+12,Front+44,PorchRoof});
        Beam(M,Pale,{XX,Front+34,PorchRoof-55},{XX+45,Front+34,PorchRoof},8,8);
    }
    Box(M,Pale,{H.L-25,Front-15,PorchRoof},{H.R+25,H.F+25,PorchRoof+22});
    Solid.Quad(TEXT("EX_FratRoof"),{H.L-35,Front-25,PorchRoof+24},{H.R+35,Front-25,PorchRoof+24},{H.R+35,H.F+20,PorchRoof+75},{H.L-35,H.F+20,PorchRoof+75},{0,-.1f,.99f},UV);
    for(float A=H.L+30;A<H.R-20;A+=42)if(FMath::Abs(A-X)>225)
    {Box(Solid,Pale,{A,Front+15,H.Floor+8},{A+4,Front+20,H.Floor+85});}
    for(float Sign:{-1.f,1.f})Box(Solid,Pale,{Sign<0?H.L+20:X+225,Front+13,H.Floor+84},{Sign<0?X-225:H.R-20,Front+24,H.Floor+92});
    Word(M,{X,H.F-33,PorchRoof+82},H.Index);
    DoorFrame(M,H,false);DoorFrame(M,H,true);
    // A high narrow brick chimney with crown cap and dark flue.
    const float CX=H.L+220,CY=H.B-240;
    Box(M,Brick,{CX-55,CY-65,Top-80},{CX+55,CY+65,Peak+80});Box(M,Stone,{CX-67,CY-77,Peak+62},{CX+67,CY+77,Peak+88});
    if(Tudor)
    {
        for(float XX:{H.L+15,X,H.R-15})Box(M,TEXT("EX_RowTimber"),{XX-9,H.F-13,H.Floor+330},{XX+9,H.F-3,Top+8});
        Beam(M,TEXT("EX_RowTimber"),{H.L+20,H.F-15,Top},{X,H.F-15,Peak-20},16,12);
        Beam(M,TEXT("EX_RowTimber"),{X,H.F-15,Peak-20},{H.R-20,H.F-15,Top},16,12);
        for(float XX=H.L+150;XX<H.R-70;XX+=210)Beam(M,TEXT("EX_RowTimber"),{XX,H.F-15,H.Floor+342},{XX+155,H.F-15,Top-30},13,10);
        Box(M,TEXT("EX_RowTimber"),{H.L,H.F-15,Top-12},{H.R,H.F-3,Top+12});
    }
    else
    {
        // The southwest tower remains inside the house body. Its usable alcove does not touch the stair.
        const FVector C(H.L+150,H.F+150,Top);const float R=150,RoofTop=Peak+280;
        // Covered west return: posts and rail stay within the approved short porch envelope.
        for(float YY:{H.F-50,H.F+350,H.F+520})Box(Solid,Pale,{H.L-145,YY-10,H.Floor},{H.L-125,YY+10,PorchRoof});
        Solid.Quad(TEXT("EX_FratRoof"),{H.L-160,H.F-155,PorchRoof+24},{H.L+12,H.F-155,PorchRoof+75},{H.L+12,H.F+565,PorchRoof+75},{H.L-160,H.F+565,PorchRoof+24},{-.2f,0,.98f},UV);
        Box(Solid,Pale,{H.L-145,H.F-85,H.Floor+84},{H.L-136,H.F+535,H.Floor+92});
        for(float YY=H.F-75;YY<H.F+520;YY+=42)Box(Solid,Pale,{H.L-142,YY,H.Floor+8},{H.L-138,YY+4,H.Floor+85});
        for(int I=0;I<8;++I)
        {
            const float A=I*PI/4,B=(I+1)*PI/4;const FVector P=C+FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,0),Q=C+FVector(FMath::Cos(B)*R,FMath::Sin(B)*R,0);
            M.Quad(Body,P-FVector(0,0,200),Q-FVector(0,0,200),Q+FVector(0,0,170),P+FVector(0,0,170),(FVector(FMath::Cos((A+B)*.5),FMath::Sin((A+B)*.5),0)),UV);
            Triangle(M,TEXT("EX_FratRoof"),P+FVector(0,0,170),Q+FVector(0,0,170),{C.X,C.Y,RoofTop},FVector(FMath::Cos((A+B)*.5)*.7,FMath::Sin((A+B)*.5)*.7,.7));
            M.Cylinder(Pale,P-FVector(0,0,205),P+FVector(0,0,170),4,6,UV);
        }
        M.Cylinder(Metal,{C.X,C.Y,RoofTop},{C.X,C.Y,RoofTop+70},2,6,UV);
        // Brackets/eave dentils and narrow contrasting corner boards.
        for(float XX=H.L+30;XX<H.R;XX+=55)Box(M,Pale,{XX,H.F-28,Top-25},{XX+9,H.F+8,Top+4});
    }
    // Period AC condenser and small cloth banner; no contemporary LEDs or logos.
    Box(M,Metal,{H.R-350,H.F-37,H.Floor+333},{H.R-230,H.F-7,H.Floor+390});
    for(float Z=H.Floor+342;Z<H.Floor+386;Z+=7)Box(M,Dark,{H.R-344,H.F-39,Z},{H.R-236,H.F-37,Z+2});
    Face(GlowMesh,TEXT("EX_Window"),{H.X()-150,H.F-20,H.Floor+165},{1,0,0},{0,-1,0},18,35);
}
}
void Walls(FHallMesh& M,const FHouse& H,FName K,float D,float Inset)
{
    TArray<FWallHole> Holes;GroundWindows(H,Holes);
    Holes.Add({0,H.X()-100,H.X()+100,H.Floor,H.Floor+228});Holes.Add({1,H.X()-70,H.X()+70,H.Floor,H.Floor+228});
    for(int S=0;S<4;++S)
    {
        const float Min=(S<2?H.L:H.F)+((H.Index==4&&(S==0||S==2))?150.f:0.f),Max=S<2?H.R:H.B,Top=H.Floor+H.Height;
        TArray<float> Bands={H.Floor,Top};
        for(const auto& O:Holes)if(O.Side==S){Bands.Add(O.Z0);Bands.Add(O.Z1);}
        Bands.Sort();
        for(int I=0;I+1<Bands.Num();++I)
        {
            const float ZA=Bands[I],ZB=Bands[I+1];if(ZB-ZA<.1f)continue;
            TArray<FVector2D> Gaps;for(const auto& O:Holes)if(O.Side==S&&O.Z0<ZB&&O.Z1>ZA)Gaps.Add({O.U0,O.U1});
            Gaps.Sort([](const FVector2D& A,const FVector2D& B){return A.X<B.X;});float U=Min;
            for(const auto& G:Gaps){WallPiece(M,H,K,S,U,float(G.X),ZA,ZB,D,Inset);U=FMath::Max(U,float(G.Y));}
            WallPiece(M,H,K,S,U,Max,ZA,ZB,D,Inset);
        }
    }
    if(H.Index==4)
    {
        const FVector C(H.L+150,H.F+150,H.Floor);const float Outer=150-Inset,Inner=Outer-D,Height=H.Height;
        for(int J=0;J<4;++J)
        {
            const float A=PI+J*PI/8,B=PI+(J+1)*PI/8;
            const FVector PA=C+FVector(FMath::Cos(A)*Outer,FMath::Sin(A)*Outer,0),PB=C+FVector(FMath::Cos(B)*Outer,FMath::Sin(B)*Outer,0);
            const FVector QA=C+FVector(FMath::Cos(A)*Inner,FMath::Sin(A)*Inner,0),QB=C+FVector(FMath::Cos(B)*Inner,FMath::Sin(B)*Inner,0),Up(0,0,Height);
            const FVector N(FMath::Cos((A+B)*.5f),FMath::Sin((A+B)*.5f),0);
            M.Quad(K,PA,PB,PB+Up,PA+Up,N,UV);M.Quad(K,QB,QA,QA+Up,QB+Up,-N,UV);
            M.Quad(K,PA+Up,PB+Up,QB+Up,QA+Up,FVector::UpVector,UV);M.Quad(K,QA,QB,PB,PA,-FVector::UpVector,UV);
            if(J==0)M.Quad(K,QA,PA,PA+Up,QA+Up,{FMath::Sin(A),-FMath::Cos(A),0},UV);
            if(J==3)M.Quad(K,PB,QB,QB+Up,PB+Up,{-FMath::Sin(B),FMath::Cos(B),0},UV);
        }
    }
}
void BuildStructures(FHallMesh& Solid,FHallMesh& Detail,FHallMesh& GlowMesh)
{
    if(!Enabled())return;
    for(const FHouse& H:Houses())
    {
        const FName Body=H.Index==0?FName(TEXT("EX_BrickRed")):H.Index==1?FName(TEXT("EX_FratSiding")):H.Index==3?FName(TEXT("EX_RowPlaster")):H.Index==4?FName(TEXT("EX_RowBlueSiding")):Brick;
        const float MinGround=FMath::Min(HouseExteriorSite::GroundHeight(H.L,H.B),HouseExteriorSite::GroundHeight(H.R,H.B))-20;
        Box(Solid,Stone,{H.L,H.F,MinGround},{H.R,H.B,H.Floor-12});
        Box(Solid,Wood,{H.L,H.F,H.Floor-12},{H.R,H.B,H.Floor});
        Walls(Solid,H,Body);
        TArray<FWallHole> W;GroundWindows(H,W);
        for(const auto& O:W)WallPiece(Solid,H,TEXT("EX_RowPane"),O.Side,O.U0,O.U1,O.Z0,O.Z1,.4f,2.f);
        PorchFloor(Solid,H);RearStair(Solid,H);
        DoorFrame(Detail,H,true);
        if(H.Index<3)
        {
            FHallMesh Cosmetic;
            const int Style=H.Index==0?2:H.Index==1?1:0;
            House(Cosmetic,H.X(),H.Y(),H.Width(),H.Depth(),H.Height+90,Style,&Solid);
            DoorFrame(Detail,H,false);
        }
        else NewHouse(Detail,Solid,GlowMesh,H);
        if(H.Index==1)Bicycle(Solid,Detail,{H.X()+H.Width()*.35f-480,H.F-160,H.Floor});
        if(H.Index==3)Bicycle(Solid,Detail,{H.R-240,H.F-170,H.Floor});
        if(H.Index>=2)
        {
            const FVector Lamp=PorchLight(H);
            const float Ceiling=H.Index==2?400.f:H.Floor+280;
            Detail.Cylinder(Metal,Lamp+FVector(0,0,14),FVector(Lamp.X,Lamp.Y,Ceiling),1.5f,6,UV);
            GlowMesh.Dome(TEXT("EX_Window"),Lamp,-FVector::UpVector,10,11,10);
            for(float X:{-12.f,12.f})for(float Y:{-12.f,12.f})
                Detail.Cylinder(Metal,Lamp+FVector(X,Y,-12),Lamp+FVector(X,Y,14),.8f,5,UV);
            Box(Detail,Metal,Lamp+FVector(-14,-14,14),Lamp+FVector(14,14,17));
        }
    }
    UE_LOG(LogTemp,Display,TEXT("FRATERNITY_ROW_STRUCTURES houses=5 hollow=5 frontDoors=200cm rearDoors=140cm frozenGradeUnchanged=1 solidTriangles=%d detailTriangles=%d"),Solid.NumTriangles(),Detail.NumTriangles());
}
}
