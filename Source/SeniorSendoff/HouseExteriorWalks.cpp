#include "HouseExteriorWalks.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"

namespace
{
const FName Concrete=TEXT("EX_WalkConcrete"), Cap=TEXT("EX_WalkCap"), Joint=TEXT("EX_WalkJoint"), Soil=TEXT("EX_WalkSoil");
const FHallUV ConcreteUV=FHallUV::World(180.f), BrickUV=FHallUV::World(120.f), PlainUV=FHallUV::World(100.f);
constexpr float CX=-36.65f, HalfStair=390.f, Front=-2190.f, GradeFront=-60.f, GradeRear=-362.f;
constexpr float Top=-2.f, Bottom=-57.f, Rise=(Top-Bottom)/3.f;

// The house walks' own copy of the grade. Every caller is inside the frozen house lot, where the Calder relief is exactly
// 0, so it carries none (HouseExteriorSite::GroundHeight is the ground everywhere else).
float Grade(float Y)
{
    return FMath::Lerp(GradeFront,GradeRear,FMath::Clamp(Y/1600.f,0.f,1.f));
}
void Box(FHallMesh& M,FName K,float X0,float X1,float Y0,float Y1,float Z0,float Z1)
{
    M.BoxMinMax(K,FVector(X0,Y0,Z0),FVector(X1,Y1,Z1),K==TEXT("EX_Brick")?BrickUV:ConcreteUV);
}
void TopQuad(FHallMesh& M,FName K,float X0,float X1,float Y0,float Y1,float Z)
{
    M.Quad(K,FVector(X0,Y0,Z),FVector(X1,Y0,Z),FVector(X1,Y1,Z),FVector(X0,Y1,Z),FVector::UpVector,ConcreteUV);
}
void Face(FHallMesh& M,FName K,const FVector& A,const FVector& B,const FVector& C,const FVector& D,const FVector& N)
{
    M.Quad(K,A,B,C,D,N,ConcreteUV);
}
void CappedBox(FHallMesh& M,float X0,float X1,float Y0,float Y1,float Z0,float Z1)
{
    // Small cast-stone chamfers catch the light without introducing a sharp projecting lip.
    constexpr float Bevel=1.5f;
    Box(M,Cap,X0,X1,Y0,Y1,Z0,Z1-Bevel);
    TopQuad(M,Cap,X0+Bevel,X1-Bevel,Y0+Bevel,Y1-Bevel,Z1);
    Face(M,Cap,FVector(X0,Y0,Z1-Bevel),FVector(X1,Y0,Z1-Bevel),FVector(X1-Bevel,Y0+Bevel,Z1),FVector(X0+Bevel,Y0+Bevel,Z1),FVector(0,-1,1).GetSafeNormal());
    Face(M,Cap,FVector(X1,Y1,Z1-Bevel),FVector(X0,Y1,Z1-Bevel),FVector(X0+Bevel,Y1-Bevel,Z1),FVector(X1-Bevel,Y1-Bevel,Z1),FVector(0,1,1).GetSafeNormal());
    Face(M,Cap,FVector(X0,Y1,Z1-Bevel),FVector(X0,Y0,Z1-Bevel),FVector(X0+Bevel,Y0+Bevel,Z1),FVector(X0+Bevel,Y1-Bevel,Z1),FVector(-1,0,1).GetSafeNormal());
    Face(M,Cap,FVector(X1,Y0,Z1-Bevel),FVector(X1,Y1,Z1-Bevel),FVector(X1-Bevel,Y1-Bevel,Z1),FVector(X1-Bevel,Y0+Bevel,Z1),FVector(1,0,1).GetSafeNormal());
}
void Step(FHallMesh& M,float Y0,float Y1,float Z)
{
    constexpr float Bevel=1.2f;
    const float X0=CX-HalfStair,X1=CX+HalfStair;
    // The nose is part of the actual collision surface, rather than a floating trim strip.
    TopQuad(M,Concrete,X0,X1,Y0+Bevel,Y1,Z);
    Face(M,Concrete,FVector(X0,Y0,Z-Bevel),FVector(X1,Y0,Z-Bevel),FVector(X1,Y0+Bevel,Z),FVector(X0,Y0+Bevel,Z),FVector(0,-1,1).GetSafeNormal());
    Face(M,Concrete,FVector(X0,Y0,-100),FVector(X1,Y0,-100),FVector(X1,Y0,Z-Bevel),FVector(X0,Y0,Z-Bevel),FVector(0,-1,0));
    Face(M,Concrete,FVector(X0,Y0,-100),FVector(X0,Y1,-100),FVector(X0,Y1,Z),FVector(X0,Y0,Z-Bevel),FVector(-1,0,0));
    Face(M,Concrete,FVector(X1,Y1,-100),FVector(X1,Y0,-100),FVector(X1,Y0,Z-Bevel),FVector(X1,Y1,Z),FVector(1,0,0));
}
void FlatJoints(FHallMesh& D,float X0,float X1,float Y0,float Y1,float Z,float Spacing)
{
    constexpr float HalfWidth=.30f;
    for(float X=FMath::CeilToFloat(X0/Spacing)*Spacing;X<X1;X+=Spacing)
        if(X>X0+4)TopQuad(D,Joint,X-HalfWidth,X+HalfWidth,Y0,Y1,Z+.06f);
    for(float Y=FMath::CeilToFloat(Y0/Spacing)*Spacing;Y<Y1;Y+=Spacing)
        if(Y>Y0+4)TopQuad(D,Joint,X0,X1,Y-HalfWidth,Y+HalfWidth,Z+.065f);
}
void Path(FHallMesh& M,FHallMesh& D,float X0,float X1,float Y0,float Y1)
{
    // Each path follows the existing front-to-basement grade. Split at the grade changes.
    TArray<float> Ys={Y0,Y1};
    for(float Y : {0.f,1600.f})if(Y>Y0&&Y<Y1)Ys.Add(Y);
    Ys.Sort();
    for(int K=0;K+1<Ys.Num();++K)
    {
        const float A=Ys[K],B=Ys[K+1],ZA=Grade(A)+3,ZB=Grade(B)+3;
        const FVector P0(X0,A,ZA),P1(X1,A,ZA),P2(X1,B,ZB),P3(X0,B,ZB);
        const FVector N=FVector::CrossProduct(P1-P0,P3-P0).GetSafeNormal();
        Face(M,Concrete,P0,P1,P2,P3,N);
        Face(M,Concrete,P0-FVector(0,0,30),P1-FVector(0,0,30),P1,P0,FVector(0,-1,0));
        Face(M,Concrete,P2-FVector(0,0,30),P3-FVector(0,0,30),P3,P2,FVector(0,1,0));
        Face(M,Concrete,P3-FVector(0,0,30),P0-FVector(0,0,30),P0,P3,FVector(-1,0,0));
        Face(M,Concrete,P1-FVector(0,0,30),P2-FVector(0,0,30),P2,P1,FVector(1,0,0));
        if(B-A>X1-X0)
        {
            for(float Y=FMath::CeilToFloat(A/300.f)*300.f;Y<B;Y+=300)
                if(Y>A+3)Face(D,Joint,FVector(X0,Y-.3f,Grade(Y-.3f)+3.06f),FVector(X1,Y-.3f,Grade(Y-.3f)+3.06f),FVector(X1,Y+.3f,Grade(Y+.3f)+3.06f),FVector(X0,Y+.3f,Grade(Y+.3f)+3.06f),N);
        }
        else for(float X=FMath::CeilToFloat(X0/300.f)*300.f;X<X1;X+=300)
            if(X>X0+3)Face(D,Joint,FVector(X-.3f,A,ZA+.06f),FVector(X+.3f,A,ZA+.06f),FVector(X+.3f,B,ZB+.06f),FVector(X-.3f,B,ZB+.06f),N);
    }
}
void Hairline(FHallMesh& D,const TArray<FVector>& Points,float Width)
{
    for(int K=0;K+1<Points.Num();++K)
    {
        const FVector A=Points[K],B=Points[K+1];
        const FVector Side=FVector(-(B-A).Y,(B-A).X,0).GetSafeNormal()*Width*.5f;
        Face(D,Joint,A-Side,B-Side,B+Side,A+Side,FVector::UpVector);
    }
}
}

void CreateHouseExteriorWalkLook(UHouseBlackHallSubsystem* L)
{
    if(!L||L->Materials.Contains(Concrete))return;
    constexpr int W=512;
    FHallImage Img;Img.Init(W,W,FLinearColor::Black);
    TArray<float> Height;Height.Init(0.f,W*W);
    for(int Y=0;Y<W;++Y)for(int X=0;X<W;++X)
    {
        // Periodic aggregate and broom finish. Joints are placed on the real slabs, not baked into every tile.
        const float Broad=HallFbm(X*.035f,Y*.035f,3,315),Grain=HallNoise(X*1.9f,Y*1.9f,319);
        const float Broom=.012f*FMath::Sin(2*PI*(Y*64.f/W+.06f*FMath::Sin(X*2*PI/W)));
        const float Pit=Grain<.095f?.09f:0.f,Pebble=Grain>.945f?.06f:0.f;
        const float V=.39f+.095f*Broad+.055f*Grain+Broom-Pit+Pebble;
        Img.At(X,Y)=FLinearColor(V,V*.988f,V*.94f);
        Height[Y*W+X]=.42f+.08f*Grain+Broom-Pit*.4f;
    }
    UTexture2D* T=HallMakeTexture(Img,true,false,TEXT("T_EX_WalkConcrete"),true);
    FHallImage N;HallHeightToNormal(Height,W,W,1.7f,true,N);
    UTexture2D* Normal=HallMakeTexture(N,false,true,TEXT("T_EX_WalkConcreteN"),true);
    if(T)L->Textures.Add(TEXT("T_EX_WalkConcrete"),T);
    if(Normal)L->Textures.Add(TEXT("T_EX_WalkConcreteN"),Normal);
    HallMakeTextured(L,Concrete,T,Normal,.88f,FLinearColor::White,.6f);
    HallMakeTextured(L,Cap,T,Normal,.82f,FLinearColor(1.02f,.97f,.88f),.42f);
    HallMakePlain(L,Joint,FLinearColor(.115f,.112f,.10f),.96f);
    HallMakePlain(L,Soil,FLinearColor(.047f,.030f,.016f),.98f);
}

void BuildHouseExteriorWalks(FHallMesh& M,FHallMesh& D)
{
    // The front concrete fills the recessed forecourt, keeping the walnut threshold at its fitted height.
    Box(M,Concrete,-1420,1360,Front,-1010,-100,Top);
    FlatJoints(D,-1416,1356,Front+2,-1012,Top,350);
    Step(M,Front-60,Front,Top-Rise);
    Step(M,Front-120,Front-60,Top-2*Rise);
    Box(M,Concrete,CX-HalfStair,CX+HalfStair,HouseExteriorSite::FrontSidewalkInnerY,Front-120,-100,Bottom);
    FlatJoints(D,CX-HalfStair,CX+HalfStair,HouseExteriorSite::FrontSidewalkInnerY,Front-122,Bottom,350);

    // Brick cheek walls and stone coping match the photo and leave the complete stair width open.
    for(const FVector2D& Span : {FVector2D(-1420,CX-HalfStair),FVector2D(CX+HalfStair,1360)})
    {
        Box(M,TEXT("EX_Brick"),Span.X,Span.Y,Front-40,Front,-90,40);
        CappedBox(M,Span.X-4,Span.Y+4,Front-45,Front+5,40,48);
        for(float X=Span.X+240;X<Span.Y-10;X+=240)TopQuad(D,Joint,X-.25f,X+.25f,Front-43,Front+3,48.065f);
    }
    for(float X : {CX-HalfStair-40,CX+HalfStair})
    {
        Box(M,TEXT("EX_Brick"),X,X+40,Front-120,Front,-90,40);
        CappedBox(M,X-4,X+44,Front-124,Front+5,40,48);
    }
    // Low soil beds leave lawn beside the approach and keep shrubs off the walking surface.
    for(const FVector2D& Span : {FVector2D(-1400,CX-HalfStair-70),FVector2D(CX+HalfStair+70,1340)})
        TopQuad(D,Soil,Span.X,Span.Y,Front-320,Front-46,GradeFront+.3f);

    // Front wings, outer sides and rear wings join the existing driveway and basement court grade.
    Path(M,D,-3550,-1420,-2390,-2200);
    Path(M,D,1360,3340,-2320,-2130);
    Path(M,D,-3550,-3400,-2200,1540);
    Path(M,D,3340,3450,-2130,-620);
    Path(M,D,3340,3540,-620,1540);
    // Two shallow end steps join the existing raised side-door landing to the perimeter path.
    Box(M,Concrete,3340,3470,-1280,-1220,-100,-31);
    Box(M,Concrete,3340,3470,-900,-840,-100,-31);
    Path(M,D,-3550,-1538,1540,1720);
    Path(M,D,1538,3540,1540,1720);

    // A few thin, irregular settlement cracks and edge wear; kept out of collision.
    Hairline(D,{FVector(-1210,-2110,Top+.08f),FVector(-1160,-2088,Top+.08f),FVector(-1138,-2041,Top+.08f),FVector(-1085,-2029,Top+.08f)},.35f);
    Hairline(D,{FVector(780,-1950,Top+.08f),FVector(819,-1983,Top+.08f),FVector(867,-1991,Top+.08f),FVector(884,-2020,Top+.08f)},.28f);
    Hairline(D,{FVector(CX+220,-2660,Bottom+.08f),FVector(CX+187,-2621,Bottom+.08f),FVector(CX+176,-2590,Bottom+.08f)},.28f);
}
