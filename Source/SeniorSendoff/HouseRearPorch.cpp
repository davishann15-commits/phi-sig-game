#include "HouseRearPorch.h"
#include "HouseBasketball.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseDetail.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
const FHallUV UV=FHallUV::World(100), WoodUV=FHallUV::World(140), BrickUV=FHallUV::World(120);
void Box(FHallMesh& M,FName K,const FVector& A,const FVector& B,const FHallUV& U=UV)
{ M.BoxMinMax(K,A,B,U); }
void Tube(FHallMesh& M,FName K,const FVector& A,const FVector& B,float R,int Seg=10)
{ M.Cylinder(K,A,B,R,Seg,UV); }
void Ring(FHallMesh& M,FName K,FVector C,float R,float Wire)
{
    for(int I=0;I<40;++I)
    {
        const float A=2*PI*I/40,B=2*PI*(I+1)/40;
        Tube(M,K,C+FVector(R*FMath::Cos(A),R*FMath::Sin(A),0),C+FVector(R*FMath::Cos(B),R*FMath::Sin(B),0),Wire,6);
    }
}
void Texture(UHouseBlackHallSubsystem* L,const TCHAR* Key,FHallImage& Img,TArray<float>& H,float Rough,float Strength)
{
    FHallImage N;HallHeightToNormal(H,Img.W,Img.H,Strength,true,N);
    const FString Base=FString::Printf(TEXT("T_%s"),Key);
    UTexture2D* T=HallMakeTexture(Img,true,false,*Base,true);
    UTexture2D* NT=HallMakeTexture(N,false,true,*(Base+TEXT("N")),true);
    L->Textures.Add(FName(*Base),T);L->Textures.Add(FName(*(Base+TEXT("N"))),NT);
    HallMakeTextured(L,FName(Key),T,NT,Rough);
}
void CreateLook(UHouseBlackHallSubsystem* L)
{
    if(!L || L->Materials.Contains(TEXT("RP_Wood")))return;
    FHallImage Img;TArray<float> H;
    Img.Init(512,512,FLinearColor::Black);H.Init(0,512*512);
    for(int Y=0;Y<512;++Y)for(int X=0;X<512;++X)
    {
        const float Knot=FMath::Sin(X*.055f+HallFbm(X*.015f,Y*.024f,3,412)*5);
        const float Grain=HallFbm(X*.09f,Y*.008f,3,82),Fine=HallNoise(X*.7f,Y*.08f,17);
        float V=.58f+.26f*Grain+.10f*Knot+.07f*Fine;
        if(X%127<2)V*=.45f;
        Img.At(X,Y)=FLinearColor(.24f,.145f,.073f)*V;
        H[Y*512+X]=.5f+.10f*Knot+.08f*Fine;
    }
    Texture(L,TEXT("RP_Wood"),Img,H,.82f,1.25f);
    HallMakeTextured(L,TEXT("RP_Pine"),L->Textures.FindRef(TEXT("T_RP_Wood")),L->Textures.FindRef(TEXT("T_RP_WoodN")),.87f,FLinearColor(1.9f,2.f,1.7f));
    Img.Init(512,512,FLinearColor::Black);H.Init(0,512*512);
    for(int Y=0;Y<512;++Y)for(int X=0;X<512;++X)
    {
        const float Stain=HallFbm(X*.018f,Y*.018f,4,692),Grit=HallNoise(X*.9f,Y*.9f,527);
        const float V=.31f+.15f*Stain+.025f*Grit;
        Img.At(X,Y)=FLinearColor(V,V*.98f,V*.91f);
        H[Y*512+X]=.5f+.04f*Grit;
    }
    FRandomStream R(318);
    for(int K=0;K<9;++K)
    {
        FVector2D P(R.FRandRange(0,512),R.FRandRange(0,512));TArray<FVector2D> Crack={P};
        for(int I=0;I<8;++I){P+=FVector2D(R.FRandRange(-10,15),R.FRandRange(5,20));Crack.Add(P);}
        Img.Stroke(Crack,.7f,FLinearColor(.055f,.05f,.043f),.55f);
    }
    Texture(L,TEXT("RP_Patio"),Img,H,.91f,.65f);
    Img.Init(512,256,FLinearColor::Black);H.Init(0,512*256);
    for(int Y=0;Y<256;++Y)for(int X=0;X<512;++X)
    {
        const float Noise=HallFbm(X*.035f,Y*.035f,3,731),Fine=HallNoise(X*.65f,Y*.65f,414);
        Img.At(X,Y)=FLinearColor(.39f,.018f,.023f)*(.8f+.3f*Noise+.06f*Fine);
        if((Y%49<1&&Fine>.65f)||(X%173<1&&Fine>.7f))Img.At(X,Y)=FLinearColor(.15f,.07f,.045f);
        H[Y*512+X]=.6f+.03f*Fine;
    }
    Texture(L,TEXT("RP_TableRed"),Img,H,.57f,.45f);
    HallMakePlain(L,TEXT("RP_Metal"),FLinearColor(.025f,.03f,.032f),.53f,.65f);
    HallMakePlain(L,TEXT("RP_Stage"),FLinearColor(.025f,.025f,.023f),.87f);
    HallMakePlain(L,TEXT("RP_Ivory"),FLinearColor(.77f,.72f,.60f),.35f);
    HallMakePlain(L,TEXT("RP_Pip"),FLinearColor(.008f,.009f,.012f),.38f);
    HallMakePlain(L,TEXT("RP_Rust"),FLinearColor(.12f,.045f,.018f),.88f);
    HallMakePlain(L,TEXT("RP_Cup"),FLinearColor(.8f,.79f,.73f),.45f);
    HallMakePlain(L,TEXT("RP_CupInside"),FLinearColor(.24f,.23f,.20f),.8f);
    HallMakePlain(L,TEXT("RP_Wicker"),FLinearColor(.038f,.029f,.018f),.77f);
    // Leather grain, panel seams and the two white end stripes follow football UVs.
    Img.Init(1024,512,FLinearColor::Black);H.Init(0,1024*512);
    for(int Y=0;Y<512;++Y)for(int X=0;X<1024;++X)
    {
        const float U=(X+.5f)/1024,V=(Y+.5f)/512;
        const float Pebble=HallNoise(X*.7f,Y*.7f,327);
        const bool Seam=FMath::Min(FMath::Frac(U*4),1-FMath::Frac(U*4))<.010f;
        const bool Stripe=FMath::Abs(V-.24f)<.017f||FMath::Abs(V-.76f)<.017f;
        Img.At(X,Y)=Stripe?FLinearColor(.73f,.68f,.53f):Seam?FLinearColor(.055f,.015f,.005f):FLinearColor(.26f,.074f,.022f)*(.8f+.25f*Pebble);
        H[Y*1024+X]=Seam?.2f:.68f+.2f*Pebble;
    }
    Texture(L,TEXT("RP_Football"),Img,H,.79f,1.3f);
    Img.Init(512,256,FLinearColor::White);H.Init(.5f,512*256);
    for(int Y=0;Y<256;++Y)for(int X=0;X<512;++X)
    {
        const float U=(X+.5f)/512,V=(Y+.5f)/256;
        const float Band=FMath::Frac(U*6+.16f*FMath::Sin(V*PI*4));
        const bool Seam=Band<.022f;
        const int Panel=int(U*6+.16f*FMath::Sin(V*PI*4));
        Img.At(X,Y)=Seam?FLinearColor(.08f,.095f,.1f):Panel%3==0?FLinearColor(.11f,.26f,.54f):Panel%3==1?FLinearColor(.69f,.55f,.055f):FLinearColor(.76f,.75f,.69f);
        H[Y*512+X]=Seam?.22f:.6f+HallNoise(X*.8f,Y*.8f,26)*.03f;
    }
    Texture(L,TEXT("RP_Volleyball"),Img,H,.72f,.7f);
}
void Cup(FHallMesh& M,FVector P,float R=3.6f,float Height=11.f)
{
    HwFrustum(M,TEXT("RP_Cup"),P,R*.70f,R,Height,28,false);
    HwFrustum(M,TEXT("RP_CupInside"),P+FVector(0,0,Height-1.5f),R*.93f,R*.96f,1.5f,28,true);
    HwDisc(M,TEXT("RP_CupInside"),P+FVector(0,0,Height-1.4f),R*.91f,28,true);
    Ring(M,TEXT("RP_Cup"),P+FVector(0,0,Height),R,.3f);
}
void Stool(FHallMesh& M,FHallMesh& D,FVector P,float Yaw)
{
    FTransform T(FRotator(0,Yaw,0),P);
    auto V=[&](float X,float Y,float Z){return T.TransformPosition(FVector(X,Y,Z));};
    for(float X:{-24.f,24.f})for(float Y:{-22.f,22.f})
        Tube(M,TEXT("RP_Metal"),V(X*1.13f,Y*1.10f,1),V(X,Y,75),1.4f);
    for(float X:{-24.f,24.f})Tube(M,TEXT("RP_Metal"),V(X,-22,25),V(X,22,25),1.2f);
    for(float Y:{-22.f,22.f})Tube(M,TEXT("RP_Metal"),V(-24,Y,25),V(24,Y,25),1.2f);
    M.Box(TEXT("RP_Wicker"),FTransform(T.GetRotation(),T.TransformPosition(FVector(0,0,76))),FVector(26,24,2),UV);
    for(float X:{-25.f,25.f})Tube(M,TEXT("RP_Metal"),V(X,22,73),V(X,31,120),1.4f);
    Tube(M,TEXT("RP_Metal"),V(-25,31,120),V(25,31,120),1.4f);
    for(int I=0;I<24;++I)
    {
        float Z=80+I*1.7f;
        Tube(D,TEXT("RP_Wicker"),V(-24,22+(Z-73)*.191f,Z),V(24,22+(Z-73)*.191f,Z),.55f,5);
    }
    for(int I=0;I<22;++I)
    {
        float X=-23+I*2.2f;
        Tube(D,TEXT("RP_Wicker"),V(X,23,79),V(X,31,119),.48f,5);
    }
}
void Bar(FHallMesh& M,FHallMesh& D)
{
    // Library end: the front is towards +X, away from the French-door landing.
    FTransform T(FRotator(0,90,0),FVector(-1240,1315,HouseRear::Floor));
    auto V=[&](float X,float Y,float Z){return T.TransformPosition(FVector(X,Y,Z));};
    auto B=[&](FHallMesh& Mesh,FName K,FVector Center,FVector Half)
    {Mesh.Box(K,FTransform(T.GetRotation(),T.TransformPosition(Center)),Half,WoodUV);};
    for(float X:{-123.f,123.f})for(float Y:{-37.f,39.f})B(M,TEXT("RP_Pine"),FVector(X,Y,53),FVector(4,4,53));
    // Separate weathered boards, an open serving cavity and a deep walnut top.
    for(int I=0;I<6;++I)B(M,TEXT("RP_Wood"),FVector(0,-41,9+I*16),FVector(127,2,7.7f));
    for(int I=0;I<6;++I)B(M,TEXT("RP_Wood"),FVector(-125,0,9+I*16),FVector(2,40,7.7f));
    B(M,TEXT("RP_Wood"),FVector(0,-25,107),FVector(134,23,3));
    B(M,TEXT("RP_Wood"),FVector(-111,16,107),FVector(23,18,3));
    B(M,TEXT("RP_Wood"),FVector(0,20,29),FVector(117,19,2));
    B(M,TEXT("RP_Stage"),FVector(0,38,62),FVector(118,2,29));
    // A small, unlettered orange accent preserves the photographed bar's look.
    B(D,TEXT("EX_Stone"),FVector(0,-43.1f,61),FVector(49,.5f,19));
    B(D,TEXT("EX_Orange"),FVector(0,-43.8f,61),FVector(45,.4f,15));
    for(float X:{-119.f,119.f})for(int I=0;I<6;++I)
        Tube(D,TEXT("RP_Metal"),V(X,-43.1f,9+I*16),V(X,-44,9+I*16),.6f,8);
    Tube(M,TEXT("RP_Metal"),V(-110,-62,22),V(110,-62,22),1.6f);
    for(float X:{-98.f,98.f})Tube(M,TEXT("RP_Metal"),V(X,-41,22),V(X,-62,22),1.2f);
    Cup(D,V(-80,-24,110));Cup(D,V(86,-24,110),3.5f,10);
    Tube(D,TEXT("EX_Green"),V(37,-24,110),V(37,-24,133),3.1f,20);
    Tube(D,TEXT("RP_Metal"),V(37,-24,133),V(37,-24,135),2.f,12);
    B(D,TEXT("EX_White"),FVector(37,-24,121),FVector(3.2f,3.2f,4));
    B(D,TEXT("RP_Metal"),FVector(-34,-24,110.7f),FVector(4,7,.7f));
    // Wall-mounted television: bracket and hanging cable have real depth.
    Box(M,TEXT("RP_Metal"),FVector(-1396,1198,HouseRear::Floor+174),FVector(-1380,1210,HouseRear::Floor+205));
    Box(M,TEXT("RP_Metal"),FVector(-1384,1157,HouseRear::Floor+164),FVector(-1378,1293,HouseRear::Floor+240));
    Box(D,TEXT("EX_Glass"),FVector(-1377.8f,1161,HouseRear::Floor+168),FVector(-1377.2f,1289,HouseRear::Floor+236));
    for(int I=0;I<10;++I)
    {
        FVector P(-1380+FMath::Sin(I*.6f)*3,1224,HouseRear::Floor+164-I*8);
        Tube(D,TEXT("RP_Metal"),P,P+FVector(1,0,-8),.55f,5);
    }
    Stool(M,D,FVector(-1070,1245,HouseRear::Floor),-90);
    Stool(M,D,FVector(-1070,1400,HouseRear::Floor),-90);
}
void Stages(FHallMesh& M,FHallMesh& D)
{
    const float F=HouseRear::Floor;
    // Two separate timber risers: the rear tier is higher, not a solid floating block.
    for(int Tier=0;Tier<2;++Tier)
    {
        const float X0=Tier?1190:1000,X1=1380,Y0=1100,Y1=1478,H=Tier?94:43;
        for(float X:{X0+8,X1-8})for(float Y:{Y0+10,Y1-10})
            Box(M,TEXT("RP_Pine"),FVector(X-3,Y-3,F),FVector(X+3,Y+3,F+H-6));
        Box(M,TEXT("RP_Stage"),FVector(X0,Y0,F+H-6),FVector(X1,Y1,F+H));
        for(float Y:{Y0,Y1-3})Box(M,TEXT("RP_Stage"),FVector(X0,Y,F+H-17),FVector(X1,Y+3,F+H));
        for(float X:{X0,X1-3})Box(M,TEXT("RP_Stage"),FVector(X,Y0,F+H-17),FVector(X+3,Y1,F+H));
        for(int J=1;J<7;++J)
            Box(D,TEXT("EX_Wood"),FVector(X0+4,Y0+(Y1-Y0)*J/7,F+H+.1f),FVector(X1-4,Y0+(Y1-Y0)*J/7+.5f,F+H+.2f));
        for(float X:{X0+12,X1-12})for(float Y:{Y0+12,Y1-12})
            Tube(D,TEXT("RP_Metal"),FVector(X,Y,F+H+.2f),FVector(X,Y,F+H+.5f),.65f,8);
    }
    Cup(D,FVector(1275,1400,F+94));
    // Low step lets a character climb onto the first tier naturally.
    Box(M,TEXT("RP_Stage"),FVector(970,1180,F),FVector(1005,1400,F+18));
}
void DieTable(FHallMesh& M,FHallMesh& D)
{
    const FVector C=HouseRear::TableCenter();
    for(float X:{-110.f,110.f})for(float Y:{-54.f,54.f})
        Box(M,TEXT("RP_Stage"),C+FVector(X-4,Y-4,0),C+FVector(X+4,Y+4,93));
    Box(M,TEXT("RP_Stage"),C+FVector(-123,-65,86),C+FVector(123,65,93));
    for(int I=0;I<4;++I)
        Box(M,TEXT("RP_TableRed"),C+FVector(-123,-65+I*32.5f,93),C+FVector(123,-33+I*32.5f,96));
    for(float Y:{-54.f,54.f})Box(M,TEXT("RP_Metal"),C+FVector(-110,Y-2,30),C+FVector(110,Y+2,34));
    for(float X:{-110.f,110.f})Box(M,TEXT("RP_Metal"),C+FVector(X-2,-54,29),C+FVector(X+2,54,33));
    // White inset lines and court-inspired blocks, without fraternity initials.
    Box(D,TEXT("EX_Line"),C+FVector(-1,-65,96.15f),C+FVector(1,65,96.25f));
    for(float X:{-114.f,114.f})Box(D,TEXT("EX_Line"),C+FVector(X-1,-55,96.15f),C+FVector(X+1,55,96.25f));
    for(float Y:{-55.f,55.f})Box(D,TEXT("EX_Line"),C+FVector(-114,Y-1,96.15f),C+FVector(114,Y+1,96.25f));
    for(float X:{-76.f,76.f})for(float Y:{-35.f,35.f})Cup(D,C+FVector(X,Y,96));
    for(float X:{-115.f,115.f})for(float Y:{-57.f,57.f})
        Tube(D,TEXT("RP_Metal"),C+FVector(X,Y,96),C+FVector(X,Y,96.5f),.65f,8);
}
void Football(FHallMesh& M,TArray<FVector>* Hull=nullptr)
{
    auto& S=M.Sec(TEXT("RP_Football"));const int Around=64,Rings=36;
    for(int J=0;J<=Rings;++J)for(int I=0;I<=Around;++I)
    {
        const float V=float(J)/Rings,A=2*PI*I/Around,P=-HALF_PI+PI*V;
        const float Radius=8.6f*FMath::Pow(FMath::Max(0.f,FMath::Cos(P)),1.22f);
        const FVector At(14.2f*FMath::Sin(P),Radius*FMath::Cos(A),Radius*FMath::Sin(A));
        const FVector N(At.X/(14.2f*14.2f),At.Y/(8.6f*8.6f),At.Z/(8.6f*8.6f));
        S.V.Add(At);S.N.Add(N.GetSafeNormal());S.UV.Add(FVector2D(float(I)/Around,V));S.T.Add(FProcMeshTangent(FVector(0,-FMath::Sin(A),FMath::Cos(A)),false));
        if(Hull && J%3==0 && I%4==0)Hull->Add(At);
    }
    for(int J=0;J<Rings;++J)for(int I=0;I<Around;++I)
    {
        const int A=J*(Around+1)+I,B=A+1,C=A+Around+1,E=C+1;
        S.I.Append({A,C,B,B,C,E});
    }
    Tube(M,TEXT("RP_Ivory"),FVector(-5,0,8.2f),FVector(5,0,8.2f),.3f,8);
    for(int I=0;I<8;++I)
    {
        const float X=-4.2f+I*1.2f,Z=8.6f*FMath::Pow(FMath::Sqrt(1-X*X/(14.2f*14.2f)),1.22f)+.15f;
        Tube(M,TEXT("RP_Ivory"),FVector(X,-1.65f,Z-.17f),FVector(X,1.65f,Z-.17f),.27f,8);
    }
}
void Die(FHallMesh& M)
{
    HwSuperBox(M,TEXT("RP_Ivory"),FTransform::Identity,FVector(1.8f),.22f,.22f,24,16,FVector2D(1,1));
    const TArray<FVector2D> Patterns[]={{{0,0}},{{-.72f,-.72f},{.72f,.72f}},{{-.72f,-.72f},{0,0},{.72f,.72f}},
        {{-.72f,-.72f},{-.72f,.72f},{.72f,-.72f},{.72f,.72f}},{{-.72f,-.72f},{-.72f,.72f},{0,0},{.72f,-.72f},{.72f,.72f}},
        {{-.72f,-.85f},{-.72f,0},{-.72f,.85f},{.72f,-.85f},{.72f,0},{.72f,.85f}}};
    const int Values[]={1,6,2,5,3,4};
    for(int Face=0;Face<6;++Face)
    {
        FVector N=FVector::ZeroVector;N[Face/2]=Face%2?-1:1;
        FVector U,V;FHallMesh::PlanarAxes(N,U,V);
        for(auto P:Patterns[Values[Face]-1])
        {
            FVector C=N*1.795f+U*P.X+V*P.Y;
            Tube(M,TEXT("RP_Pip"),C,C+N*.012f,.235f,16);
        }
    }
}
}

void BuildHouseRearPorchDetail(FHallMesh& Solid,FHallMesh& Decor,UHouseBlackHallSubsystem* L)
{
    CreateLook(L);Bar(Solid,Decor);Stages(Solid,Decor);
}
void BuildHouseRearCourtDetail(FHallMesh& Solid,FHallMesh& Decor,UHouseBlackHallSubsystem* L)
{
    CreateLook(L);const float F=HouseRear::Floor;
    // Remove the turf gap entirely: slab and expansion joint meet the arcade and court.
    Box(Solid,TEXT("RP_Patio"),FVector(-1470,1719,F-40),FVector(1470,1901,F),FHallUV::World(256));
    // The side strips run 2 cm under the court walls' inner face (+/-1472); stopping at +/-1470 left a 2 cm slit to the void.
    Box(Solid,TEXT("EX_Concrete"),FVector(-1474,1901,F-40),FVector(-1380,3820,F),FHallUV::World(256));
    Box(Solid,TEXT("EX_Concrete"),FVector(1380,1901,F-40),FVector(1474,3820,F),FHallUV::World(256));
    Box(Solid,TEXT("EX_Concrete"),FVector(-1380,3740,F-40),FVector(1380,3820,F),FHallUV::World(256));
    // The gate pad meets the porch-end block's north face (Y 1760); from 1768 it left a 36 x 8 cm hole by the hinge post.
    Box(Solid,TEXT("EX_Concrete"),FVector(1430,1760,F-30),FVector(1640,1924,F),FHallUV::World(256));
    for(float X:{-980.f,-490.f,0.f,490.f,980.f})
        Box(Decor,TEXT("EX_Wood"),FVector(X-.5f,1720,F+.05f),FVector(X+.5f,1900,F+.15f));
    Box(Decor,TEXT("EX_Wood"),FVector(-1468,1898,F+.05f),FVector(1468,1899,F+.15f));
    // Library-side gap becomes a continuous brick wall with a stone cap.
    Box(Solid,TEXT("EX_Brick"),FVector(-1516,1720,F-40),FVector(-1464,1976,F+328),BrickUV);
    Box(Solid,TEXT("EX_Stone"),FVector(-1523,1720,F+328),FVector(-1457,1976,F+340));
    // The kitchen-side opening retains the photo's narrow metal frame and railing above.
    for(float Y:{1771.f,1919.f})Box(Solid,TEXT("RP_Metal"),FVector(1486,Y-4,F),FVector(1494,Y+4,F+258));
    Box(Solid,TEXT("RP_Metal"),FVector(1486,1768,F+252),FVector(1494,1924,F+258));
    Box(Solid,TEXT("RP_Metal"),FVector(1488,1768,F+328),FVector(1492,1924,F+332));
    for(float Y=1782;Y<1919;Y+=15)Box(Solid,TEXT("RP_Metal"),FVector(1488,Y,F+258),FVector(1492,Y+2,F+332));
    DieTable(Solid,Decor);
}

AHouseCourtDoor::AHouseCourtDoor()
{
    DoorId=TEXT("KitchenCourtDoor");bLocalLeafSpace=true;ClosedYaw=0;OpenYaw=-98;
    DoorwayCenter=HouseRear::GateCenter();bAlwaysRelevant=true;
    Tags.Add(TEXT("SSORearPorchCourt"));
}
void AHouseCourtDoor::BeginPlay()
{
    if(auto* L=GetWorld()->GetSubsystem<UHouseBlackHallSubsystem>())
    {
        CreateLook(L);FHallMesh M;
        for(float Y:{2.f,HouseRear::GateWidth-2})Box(M,TEXT("RP_Metal"),FVector(-2.2f,Y-2,5),FVector(2.2f,Y+2,252));
        for(float Z:{7.f,129.f,250.f})Box(M,TEXT("RP_Metal"),FVector(-2.2f,0,Z-2),FVector(2.2f,HouseRear::GateWidth,Z+2));
        Box(M,TEXT("RP_Metal"),FVector(-1.3f,3,9),FVector(1.3f,HouseRear::GateWidth-3,126));
        for(float Y=10;Y<HouseRear::GateWidth-4;Y+=11)Box(M,TEXT("RP_Metal"),FVector(-1,Y,132),FVector(1,Y+1.8f,248));
        for(float Z:{35.f,215.f})Tube(M,TEXT("RP_Metal"),FVector(0,0,Z-4),FVector(0,0,Z+4),4,16);
        for(float X:{-5.f,5.f})Tube(M,TEXT("EX_Metal"),FVector(X,HouseRear::GateWidth-11,122),FVector(X,HouseRear::GateWidth-25,122),1.5f,16);
        for(float Z:{25.f,110.f})Box(M,TEXT("RP_Rust"),FVector(1.31f,5,Z),FVector(1.40f,12,Z+2));
        OriginalLeaf=HwBuildStaticMesh(M,TEXT("SM_KitchenCourtDoor"),L,{FBox(FVector(-2.2f,0,5),FVector(2.2f,HouseRear::GateWidth,252))});
        OnConstruction(GetActorTransform());
    }
    Super::BeginPlay();
}

AHouseCourtProp::AHouseCourtProp()
{
    CarrySize=ESSOCarrySize::Small;bMovable=true;bBreakable=false;bPhysicsActive=true;
    bAlwaysRelevant=true;SetNetUpdateFrequency(45);SetMinNetUpdateFrequency(15);
    Tags.Add(TEXT("SSORearPorchCourt"));Tags.Add(TEXT("SSOCourtSmallProp"));
}
void AHouseCourtProp::BeginPlay()
{
    if(auto* L=GetWorld()->GetSubsystem<UHouseBlackHallSubsystem>())
    {
        CreateLook(L);FHallMesh M;TArray<FBox> Col;TArray<FVector> Hull;
        if(Item==EHouseCourtItem::Football)
        {
            Football(M,&Hull);PropId=TEXT("CourtFootball");PropName=TEXT("football");MassKg=.42f;
            IntactMesh=HwBuildStaticMesh(M,TEXT("SM_CourtFootball"),L,{});
            if(IntactMesh && IntactMesh->GetBodySetup())
            {
                auto* BS=IntactMesh->GetBodySetup();FKConvexElem E;E.VertexData=Hull;E.UpdateElemBox();BS->AggGeom.ConvexElems.Add(E);
                BS->InvalidatePhysicsData();BS->CreatePhysicsMeshes();
            }
        }
        else if(Item==EHouseCourtItem::Die)
        {
            Die(M);PropId=TEXT("CourtDie");PropName=TEXT("die");MassKg=.009f;
            IntactMesh=HwBuildStaticMesh(M,TEXT("SM_CourtDie"),L,{FBox(FVector(-1.76f),FVector(1.76f))});
        }
        else
        {
            IntactMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
            MaterialOverrides={L->Mat(TEXT("RP_Volleyball"))};PropId=TEXT("CourtVolleyball");PropName=TEXT("volleyball");MassKg=.27f;
        }
        Body->SetStaticMesh(IntactMesh);
    }
    Super::BeginPlay();
    Surface=NewObject<UPhysicalMaterial>(this);Surface->Friction=.52f;Surface->StaticFriction=.6f;
    Surface->Restitution=Item==EHouseCourtItem::Die?.18f:Item==EHouseCourtItem::Football?.38f:.65f;
    Surface->bOverrideRestitutionCombineMode=true;Surface->RestitutionCombineMode=EFrictionCombineMode::Max;
    Body->SetPhysMaterialOverride(Surface);Body->SetUseCCD(true);
    Body->SetLinearDamping(.12f);Body->SetAngularDamping(.45f);Body->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
    Body->SetCanEverAffectNavigation(false);SetActorTickEnabled(false);
}
float AHouseCourtProp::TakeDamage(float Amount,const FDamageEvent& Event,AController* Instigator,AActor* Causer){return 0;}
void AHouseCourtProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const
{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(AHouseCourtProp,Item);}

void SpawnHouseRearCourtItems(UWorld* W)
{
    if(!W || W->GetNetMode()==NM_Client)return;
    FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    W->SpawnActor<AHouseCourtDoor>(AHouseCourtDoor::StaticClass(),FTransform(HouseRear::GateHinge()),P);
    for(auto At:{FVector(-890,3370,HouseRear::Floor+16),FVector(950,2130,HouseRear::Floor+16)})
        W->SpawnActor<AHouseBasketball>(AHouseBasketball::StaticClass(),FTransform(FRotator::ZeroRotator,At,FVector(.248f)),P);
    auto Spawn=[&](EHouseCourtItem Kind,FVector At,FRotator Rot,FVector Scale=FVector::OneVector)
    {
        const FTransform T(Rot,At,Scale);
        auto* A=W->SpawnActorDeferred<AHouseCourtProp>(AHouseCourtProp::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if(A){A->SetFlags(RF_Transient);A->Item=Kind;A->FinishSpawning(T);}
    };
    Spawn(EHouseCourtItem::Football,HouseRear::FootballHome(),FRotator(0,24,0));
    Spawn(EHouseCourtItem::Volleyball,FVector(-950,2310,HouseRear::Floor+13),FRotator(0,67,0),FVector(.21f));
    Spawn(EHouseCourtItem::Die,HouseRear::TableCenter()+FVector(-24,14,99),FRotator(0,23,0));
    Spawn(EHouseCourtItem::Die,HouseRear::TableCenter()+FVector(30,-12,99),FRotator(0,-16,0));
}
