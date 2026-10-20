// Candidate only: integrate after the unchanged-scene baseline. No game/editor jobs are run by this file.
#include "HouseFraternityRowYards.h"
#include "HouseFraternityRow.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseCampusStaticMesh.h"
#include "HouseExteriorSiteLayout.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

namespace HouseFraternityRowYards
{
namespace
{
const FName Wood(TEXT("RY_Wood")), Paint(TEXT("RY_Paint")), Metal(TEXT("RY_Metal")), Rust(TEXT("RY_Rust")),
    Cloth(TEXT("RY_Cloth")), Cream(TEXT("RY_Cream")), Red(TEXT("RY_Red")), Dark(TEXT("RY_Dark")),
    Stone(TEXT("RY_Stone")), Mud(TEXT("RY_Mud")), Sand(TEXT("RY_Sand")), Grass(TEXT("RY_Grass")),
    Green(TEXT("RY_Green")), Leaf(TEXT("RY_Leaf")), LeafGold(TEXT("RY_LeafGold")), GlowKey(TEXT("RY_Glow"));
const FHallUV UV=FHallUV::World(80);
struct FRect { float L,R,F,B; bool Contains(FVector2D P,float Radius=0) const {return P.X>L-Radius&&P.X<R+Radius&&P.Y>F-Radius&&P.Y<B+Radius;} };
const FRect Sheds[]={{-16680,-16180,3950,4330},{-14900,-14300,3920,4350},{-10500,-9750,3850,4380},
    {-8240,-7790,3890,4320},{-5140,-4700,3970,4350}};
const float Parcel[]={-18740,-15750,-12000,-8500,-6700,-3900};
const FVector2D FixedRoute[]={{-4500,2900},{-5700,3200},{-7050,3800},{-7050,4200},{-7050,4830},{-7050,5480},{-6800,5900}};
const FVector2D Access[]={{-8100,-3900},{-8100,0},{-6550,0},{-6550,2200},{-6350,3100},{-7050,3100},{-7050,4520}};
const FVector2D Commons[]={{-5510,-3900},{-5510,-2800},{-5510,-1400},{-4320,-1400},{-4320,-600},{-4320,-195},{-3480,-195}};
const FVector2D TudorApproach[]={{-5510,-2800},{-7600,-2800},{-7600,122}};
// TudorApproach already draws the shared Commons-to-branch leg atY-2800.
const FVector2D AnneApproach[]={{-6380,-2800},{-6380,-2020}};
const TArray<TArray<FVector2D>>& YardPaths()
{
    static const TArray<TArray<FVector2D>> Paths={
        {{-17400,85},{-17400,900},{-17100,1450},{-17100,3300},{-16900,4400}},
        {{-13900,-50},{-13900,900},{-13650,2300},{-13650,4400}},
        {{-10100,-20},{-10100,550},{-9700,1900},{-9350,3300},{-9350,4400}},
        {{-7600,2350},{-7600,2700},{-7400,3100},{-7400,3620},{-7500,4400}},
        {{-6300,300},{-6300,700},{-5500,900},{-5500,2850},{-5700,3750},{-5550,4400}}};
    return Paths;
}
// Native tree roots archived by the accepted WoodsPass. This is an exclusion list only; no tree is edited/hidden.
const FVector2D OriginalRoots[]={{-4200,1200},{-18935.226562f,1610.536621f},{-6215.250488f,1596.282227f},
    {-18826.691406f,3805.634766f},{-8355.871094f,-3800},{-4800,-2600},
    {-17851.960938f,-3800},{-15474.238281f,-3800},{-13220.230469f,-3800},{-10988.771484f,-3800}};
float Grade(FVector2D P){return HouseExteriorSite::GroundHeight(P.X,P.Y);}
FVector At(FVector2D P,float Lift=0){return FVector(P.X,P.Y,Grade(P)+Lift);}
void Box(FHallMesh& M,FName K,FVector A,FVector B){M.BoxMinMax(K,A,B,UV);}
void GroundBox(FHallMesh& M,FName K,FVector2D P,FVector Half,float Yaw=0,float Lift=0)
{M.Box(K,FTransform(FRotator(0,Yaw,0),At(P,Half.Z+Lift)),Half,UV);}
void Beam(FHallMesh& M,FName K,FVector A,FVector B,float Width=4)
{
    const FVector D=B-A;M.Box(K,FTransform(D.Rotation(),(A+B)*.5f),FVector(D.Size()*.5f,Width*.5f,Width*.5f),UV);
}
float SegmentDistance(FVector2D P,FVector2D A,FVector2D B)
{const FVector2D D=B-A;const float T=FMath::Clamp(FVector2D::DotProduct(P-A,D)/FMath::Max(D.SizeSquared(),1.),0.,1.);return FVector2D::Distance(P,A+D*T);}
template <int32 N> bool NearPath(FVector2D P,const FVector2D (&Path)[N],float Radius)
{for(int32 I=1;I<N;++I)if(SegmentDistance(P,Path[I-1],Path[I])<Radius)return true;return false;}
bool NearAnyPath(FVector2D P,float Radius)
{
    if(NearPath(P,FixedRoute,Radius)||NearPath(P,Access,Radius)||NearPath(P,Commons,Radius)||
        NearPath(P,TudorApproach,Radius)||NearPath(P,AnneApproach,Radius))return true;
    for(const TArray<FVector2D>& Path:YardPaths())for(int32 I=1;I<Path.Num();++I)
        if(SegmentDistance(P,Path[I-1],Path[I])<Radius)return true;
    return false;
}
bool Clear(FVector2D P,float Radius=0,bool bAvoidAllPaths=true)
{
    if(P.X<-18730+Radius||P.X>-3900-Radius||P.Y>4470-Radius)return false;
    for(float X:{-17500.f,-12500.f,-7000.f,-4400.f})for(float Y:{2100.f,2800.f,3700.f})
        if(FVector2D::Distance(P,{X,Y})<120+Radius)return false;
    for(const FVector2D& Root:OriginalRoots)if(FVector2D::Distance(P,Root)<220+Radius)return false;
    if(NearPath(P,FixedRoute,120+Radius))return false;
    if(bAvoidAllPaths&&NearAnyPath(P,110+Radius))return false;
    for(const auto& H:HouseFraternityRow::Houses())if(FRect{H.L-35,H.R+35,H.F-35,H.B+35}.Contains(P,Radius))return false;
    for(const FRect& R:Sheds)if(R.Contains(P,Radius+60))return false;
    // Exact rear steps and original front entrances are reserved, not merely a point at the door.
    const FRect Steps[]={{-17600,-17200,-295,85},{-14100,-13700,-430,-50},{-10300,-9900,-400,-20},
        {-7800,-7400,122,450},{-7800,-7400,1850,2350}};
    for(const FRect& R:Steps)if(R.Contains(P,Radius))return false;
    const auto& Anne=HouseFraternityRow::Houses()[4];const float Front=Anne.F-150;
    // R5's moved porch/west return and six-riser entry follow the permanent descriptor, not the former lot.
    const FRect AnneFloors[]={{Anne.L,Anne.R,Front,Anne.F},{Anne.L-150,Anne.L,Front,Anne.F+550},
        {Anne.X()-200,Anne.X()+200,Front-140,Front},
        {HouseFraternityRow::RearStairX(Anne)-200,HouseFraternityRow::RearStairX(Anne)+200,Anne.B-3,Anne.B+500},
        {Anne.L+121,Anne.L+339,Anne.F-105,Anne.F-45}};
    for(const FRect& R:AnneFloors)if(R.Contains(P,Radius))return false;
    const FRect Furnishings[]={
        {-18400,-17780,1300,1810},{-18400,-18000,2380,2820},{-16650,-16340,2780,2930},
        {-15400,-14530,320,1100},{-15240,-14890,2390,2850},{-14450,-14320,2600,2780},
        {-11640,-10560,1040,2770},{-11550,-10550,2990,3710},{-11020,-10680,340,570},
        {-11770,-11240,200,900},{-8930,-8770,2990,3160},
        {-8360,-7720,2420,3000},{-8320,-7870,3030,3460},{-8280,-8120,2240,2380},
        {-5030,-4670,2350,2850},{-6080,-5620,2410,2640},
        {-8730,-8270,-1020,-680},{-6100,-5800,-2550,-2350}};
    for(const FRect& R:Furnishings)if(R.Contains(P,Radius))return false;
    for(float X:{-15750.f,-12000.f})for(FVector2D G:{FVector2D(700,1150),FVector2D(3100,3550)})
        if(FRect{X-120,X+120,float(G.X-40),float(G.Y+40)}.Contains(P,Radius))return false;
    if(FRect{-8620,-8380,60,690}.Contains(P,Radius)||FRect{-8620,-8380,3060,3640}.Contains(P,Radius)||
        FRect{-6820,-6580,1010,2190}.Contains(P,Radius)||FRect{-6820,-6580,2960,3960}.Contains(P,Radius))return false;
    return true;
}
// Continuous wear ribbon subdivided at <=75cm: samples the existing grade, no collision or terrain mutation.
void Wear(FHallMesh& D,FVector2D A,FVector2D B,float Width,FName K=Mud,float Lift=.25f)
{
    const FVector2D V=B-A,N(-V.Y,V.X),Side=N.GetSafeNormal();
    const bool Soil=K==Mud;const FName Material=Soil?FName(TEXT("EX_WoodsTrail")):K;
    auto EdgeWidth=[&](FVector2D P)
    {return Width*.5f*(Soil?.86f+.09f*FMath::Sin(P.X*.017f+P.Y*.011f)+.05f*FMath::Sin(P.Y*.039f-P.X*.007f):1.f);};
    const int32 Count=FMath::Max(1,FMath::CeilToInt(V.Size()/75));
    for(int32 I=0;I<Count;++I)
    {
        const FVector2D P=FMath::Lerp(A,B,float(I)/Count),Q=FMath::Lerp(A,B,float(I+1)/Count);
        // Existing dirt/grass blend softens the worn edge; all original route centres and grade stay exact.
        D.QuadUV(Material,At(P-Side*EdgeWidth(P),Lift),At(Q-Side*EdgeWidth(Q),Lift),
            At(Q+Side*EdgeWidth(Q),Lift),At(P+Side*EdgeWidth(P),Lift),FVector::UpVector,
            {V.Size()*I/Count/180.f,0},{V.Size()*(I+1)/Count/180.f,0},
            {V.Size()*(I+1)/Count/180.f,1},{V.Size()*I/Count/180.f,1},FVector(Side.Y,-Side.X,0));
    }
}
template<int32 N>void Path(FHallMesh& D,const FVector2D (&Points)[N],float Width)
{for(int32 I=1;I<N;++I)Wear(D,Points[I-1],Points[I],Width);}
void Paving(FHallMesh& D,FRect R,FName K,float Tile=65)
{
    for(float X=R.L;X<R.R;X+=Tile)for(float Y=R.F;Y<R.B;Y+=Tile)
    {
        const float X1=FMath::Min(X+Tile-2,R.R),Y1=FMath::Min(Y+Tile-2,R.B);
        D.Quad(K,At({X,Y},.3f),At({X1,Y},.3f),At({X1,Y1},.3f),At({X,Y1},.3f),FVector::UpVector,UV);
    }
}
void Post(FHallMesh& S,FVector2D P,float Height,FName K)
{GroundBox(S,K,P,{4.5f,4.5f,Height*.5f});}
void Divider(FHallMesh& S,FHallMesh& D,float X,int32 Kind,const TArray<FVector2D>& Gaps)
{
    // Each closed interval is bounded exactly; no diagonal "gate" intrudes into a reserved opening.
    TArray<float> Ends={100};for(const FVector2D& G:Gaps){Ends.Add(G.X);Ends.Add(G.Y);}Ends.Add(4420);
    for(int32 I=0;I+1<Ends.Num();I+=2)
    {
        const float Start=Ends[I],End=Ends[I+1];if(End<=Start)continue;
        const float Height=Kind==0?100:Kind==1?135:Kind==2?105:110;
        const FName K=Kind==1?Metal:Kind==0?Paint:Wood;
        const int32 Panels=FMath::Max(1,FMath::CeilToInt((End-Start)/230));
        for(int32 J=0;J<=Panels;++J)Post(S,{X,FMath::Lerp(Start+5,End-5,float(J)/Panels)},Height,K);
        for(int32 J=0;J<Panels;++J)
        {
            const float A=FMath::Lerp(Start,End,float(J)/Panels),B=FMath::Lerp(Start,End,float(J+1)/Panels);
            const float Z=FMath::Min(Grade({X,A}),Grade({X,B}));
            if(Kind==1)
            {
                // Real colliding wire lattice; there is no opaque sheet or invisible wall behind it.
                for(float H:{18.f,128.f})Beam(S,Metal,{X,A,Z+H},{X,B,Z+H},3);
                for(float Y=A;Y<B;Y+=24)
                {const float H=FMath::Min(B-Y,116.f);Beam(S,Metal,{X-2,Y,Z+10},{X-2,Y+H,Z+10+H},.7f);Beam(S,Metal,{X-2,Y,Z+126},{X-2,Y+H,Z+126-H},.7f);}
            }
            else
            {
                for(float H:{36.f,86.f})Beam(S,K,{X,A,Z+H},{X,B,Z+H},7);
                if(Kind==0)for(float Y=A+10;Y<B-4;Y+=15)
                {const float G=Grade({X,Y});Box(S,Paint,{X-2,Y-4,G+10},{X+2,Y+4,G+95});
                    D.Prism(Paint,{{-4,0},{4,0},{0,9}},{X-2,Y,G+95},{0,1,0},{0,0,1},{1,0,0},4,UV);}
                // Last divider's growth stays west of its rails, clear of Access and the unchanged diagonal route.
                if(Kind==3)for(float Y=A+20;Y<B;Y+=90)
                    D.Dome(Green,At({X-42,Y},25),FVector::UpVector,36,68,7);
            }
        }
    }
    // Open latch plates explain deliberate side gates; the full gap remains clear.
    for(const FVector2D& G:Gaps)
    {const float Z=Grade({X,G.X});Box(D,Rust,{X-7,G.X-10,Z+65},{X+7,G.X-6,Z+74});}
}
void Shed(FHallMesh& S,FHallMesh& D,int32 I)
{
    const FRect R=Sheds[I];const float Z=FMath::Max(FMath::Max(Grade({R.L,R.F}),Grade({R.R,R.F})),FMath::Max(Grade({R.L,R.B}),Grade({R.R,R.B})));
    const auto ShedBox=[&](FHallMesh& M,FName K,FVector2D P,FVector Half,float Lift=.5f)
    {M.Box(K,FTransform(FVector(P.X,P.Y,Z+Half.Z+Lift)),Half,UV);};
    const float C=(R.L+R.R)*.5f,Door=I==2?145.f:75.f,H=I==2?285.f:240.f;
    // Walk-in openings 150cm (garage290cm) wide and210cm high. Floor meets unchanged grade.
    Box(S,Stone,{R.L,R.F,Z-6},{R.R,R.B,Z+.5f});
    Box(S,Wood,{R.L,R.F,Z},{R.L+12,R.B,Z+H});Box(S,Wood,{R.R-12,R.F,Z},{R.R,R.B,Z+H});
    Box(S,Wood,{R.L,R.B-12,Z},{R.R,R.B,Z+H});
    if(I!=3)
    {Box(S,I==1?Paint:Wood,{R.L,R.F,Z},{C-Door,R.F+12,Z+H});Box(S,I==1?Paint:Wood,{C+Door,R.F,Z},{R.R,R.F+12,Z+H});
        Box(S,Wood,{C-Door,R.F,Z+215},{C+Door,R.F+12,Z+H});}
    else // The lean-to is open across its full southern face.
    {Post(S,{R.L+5,R.F+5},H,Wood);Post(S,{R.R-5,R.F+5},H,Wood);}
    // Differing lean-to/hipped-looking roof silhouettes, rafters, plank seams and weathered trim.
    const float Ridge=Z+H+(I==3?10:75),Eave=Z+H;
    if(I==3)D.Quad(Rust,{R.L-22,R.F-25,Eave},{R.R+22,R.F-25,Eave},{R.R+22,R.B+25,Ridge+35},{R.L-22,R.B+25,Ridge+35},{0,-.2f,.98f},UV);
    else
    {D.Quad(Dark,{R.L-22,R.F-25,Eave},{C,R.F-25,Ridge},{C,R.B+25,Ridge},{R.L-22,R.B+25,Eave},{-.25f,0,.97f},UV);
        D.Quad(Dark,{C,R.F-25,Ridge},{R.R+22,R.F-25,Eave},{R.R+22,R.B+25,Eave},{C,R.B+25,Ridge},{.25f,0,.97f},UV);
        for(float Y:{R.F,R.B-12})S.Prism(Wood,{{R.L,0},{R.R,0},{C,Ridge-Eave}},{0,Y,Eave},{1,0,0},{0,0,1},{0,1,0},12,UV);}
    // Roof underside is solid overhead. These walk-in structures do not have invisible blockers.
    Box(S,Wood,{R.L,R.F,Z+H-7},{R.R,R.B,Z+H});
    for(float X=R.L+30;X<R.R;X+=22)Beam(D,I==1?Paint:Wood,{X,R.B+.8f,Z+8},{X,R.B+.8f,Z+H-5},1.2f);
    for(float X:{R.L+30,R.R-30})Beam(D,Wood,{X,R.F+20,Z+H-14},{X,R.B-10,Z+H-14},12);
    // Open doors lie flat against the facade, not across either the opening or its outside approach.
    if(I!=3)for(float Side:{-1.f,1.f})
    {const float X=C+Side*(Door+(I==2?64:32));ShedBox(S,I==1?Paint:Wood,{X,R.F-9},{I==2?60.f:29.f,4,105});
        for(float Hinge:{40.f,170.f})Box(D,Rust,{X-12,R.F-15,Z+Hinge},{X+12,R.F-12,Z+Hinge+5});}
    // Workbench occupies the back wall, leaving180+cm of clear depth from the entry.
    const float BenchX=R.R-110,BenchY=R.B-65;
    ShedBox(S,Wood,{BenchX,BenchY},{88,32,4},80);
    for(float X:{BenchX-72,BenchX+72})ShedBox(S,Wood,{X,BenchY},{4,27,40});
    for(int32 K=0;K<3;++K)ShedBox(D,Dark,{BenchX-58+K*45,BenchY},{15,21,13},88);
    if(I==0) // Folded old deck chairs and hanging garden tools.
    {for(int32 K=0;K<3;++K)Beam(D,Rust,{R.L+35,R.B-50-K*40,Z+8},{R.L+70,R.B-50-K*40,Z+125},5);}
    else if(I==1)
    {for(int32 K=0;K<4;++K){const FVector A(R.L+45+K*38,R.B-18,Z+40),B=A+FVector(0,0,130);Beam(D,Wood,A,B,3);D.Cylinder(Metal,A,A+FVector(0,-3,-25),10,7,UV);}}
    else if(I==2)
    {ShedBox(S,Dark,{R.L+80,R.B-80},{50,28,48});for(int32 K=0;K<3;++K)ShedBox(D,Rust,{R.L+65+K*36,R.B-75},{13,17,18},96.5f);}
    else if(I==3)
    {for(int32 K=0;K<5;++K)D.Cylinder(Wood,{R.L+45+K*28,R.B-75,Z+20},{R.L+45+K*28,R.B-20,Z+20},11,7,UV);}
    else
    {for(int32 K=0;K<2;++K)ShedBox(S,Dark,{R.L+65,R.B-70-K*75},{38,30,35});}
    Wear(D,{C,R.F-230},{C,R.F},180,I==1?Stone:Mud);
}
void Picnic(FHallMesh& S,FHallMesh& D,FVector2D P,float Yaw=0)
{
    const FTransform T(FRotator(0,Yaw,0),At(P));auto B=[&](FName K,FVector C,FVector H){S.Box(K,FTransform(T.GetRotation(),T.TransformPosition(C)),H,UV);};
    // Five individual planks, splayed trestles and two real benches, seating scale in centimetres.
    for(int32 I=0;I<5;++I)B(Wood,{0,-35+I*17.5f,76},{120,8,3});
    for(float Y:{-75.f,75.f}){B(Wood,{0,Y,43},{120,17,3});for(float X:{-80.f,80.f})B(Wood,{X,Y*.58f,23},{4,4,23});}
    for(float X:{-82.f,82.f}){Beam(S,Wood,T.TransformPosition({X,-60,2}),T.TransformPosition({X,-27,73}),8);Beam(S,Wood,T.TransformPosition({X,60,2}),T.TransformPosition({X,27,73}),8);}
    for(float X:{-80.f,80.f})for(float Y:{-32.f,32.f})
        D.Cylinder(Metal,T.TransformPosition({X,Y,79}),T.TransformPosition({X,Y,80}),1.5f,6,UV);
}
void FoldingTable(FHallMesh& S,FHallMesh& D,FVector2D P,float Yaw=0)
{
    const FTransform T(FRotator(0,Yaw,0),At(P));S.Box(Cream,FTransform(T.GetRotation(),T.TransformPosition({0,0,74})),{120,37,2.5f},UV);
    for(float X:{-85.f,85.f})for(float Side:{-1.f,1.f})Beam(S,Metal,T.TransformPosition({X,Side*29,2}),T.TransformPosition({X,Side*24,72}),3);
    Beam(D,Metal,T.TransformPosition({-85,0,35}),T.TransformPosition({85,0,35}),2);
}
void Grill(FHallMesh& S,FHallMesh& D,FVector2D P)
{
    const FVector B=At(P);D.Dome(Dark,B+FVector(0,0,78),-FVector::UpVector,38,25,12);
    S.Cylinder(Dark,B+FVector(0,0,54),B+FVector(0,0,79),35,12,UV);
    for(int32 I=0;I<3;++I){const float A=I*2*PI/3;Beam(S,Metal,B+FVector(23*FMath::Cos(A),23*FMath::Sin(A),60),B+FVector(36*FMath::Cos(A),36*FMath::Sin(A),3),3);}
    for(float X=-25;X<=25;X+=8){const float W=FMath::Sqrt(FMath::Max(0.f,900-X*X));Beam(D,Metal,B+FVector(X,-W,80),B+FVector(X,W,80),1.4f);}
    D.Dome(Dark,B+FVector(60,0,10),FVector::UpVector,39,31,12);Beam(D,Wood,B+FVector(42,-15,42),B+FVector(78,-15,42),4);
}
void Couch(FHallMesh& S,FHallMesh& D,FVector2D P,float Yaw,bool Fallen=false)
{
    const FTransform T(FRotator(Fallen?-12:0,Yaw,0),At(P,8));
    auto B=[&](FName K,FVector C,FVector H){S.Box(K,FTransform(T.GetRotation(),T.TransformPosition(C)),H,UV);};
    B(Cloth,{0,0,23},{111,43,18});B(Cloth,{0,36,62},{111,12,39});for(float X:{-110.f,110.f})B(Cloth,{X,0,49},{12,48,34});
    for(float X:{-73.f,0.f,73.f})B(Cloth,{X,-5,45},{34,32,10});
    for(float X:{-84.f,84.f})for(float Y:{-30.f,30.f})B(Wood,{X,Y,2},{5,5,10});
    D.Box(Dark,FTransform(T.GetRotation(),T.TransformPosition({27,-9,56})),{18,12,.25f},UV);
}
void Chair(FHallMesh& S,FHallMesh& D,FVector2D P,float Yaw,bool Fallen=false)
{
    const FTransform T(FRotator(Fallen?65:0,Yaw,0),At(P,Fallen?36:0));
    S.Box(Cloth,FTransform(T.GetRotation(),T.TransformPosition({0,0,43})),{27,25,2},UV);
    S.Box(Cloth,FTransform(T.GetRotation(),T.TransformPosition({0,24,70})),{27,2,24},UV);
    for(float X:{-27.f,27.f})for(float Side:{-1.f,1.f})Beam(S,Metal,T.TransformPosition({X,Side*22,2}),T.TransformPosition({X,-Side*20,46}),2);
    Beam(D,Metal,T.TransformPosition({-28,25,94}),T.TransformPosition({28,25,94}),2);
}
void Keg(FHallMesh& S,FHallMesh& D,FVector2D P)
{
    const FVector B=At(P);S.Cylinder(Metal,B+FVector(0,0,1),B+FVector(0,0,58),21,12,UV);
    for(float Z:{7.f,15.f,43.f,51.f})D.Cylinder(Metal,B+FVector(0,0,Z),B+FVector(0,0,Z+3),23,12,UV);
    Beam(D,Dark,B+FVector(0,0,57),B+FVector(0,0,76),4);Beam(D,Metal,B+FVector(0,0,75),B+FVector(11,0,75),3);
    D.Cylinder(Dark,B+FVector(0,0,0),B+FVector(0,0,25),35,12,UV,false);
    for(int32 I=0;I<10;++I){const float A=I*2*PI/10;GroundBox(D,Cream,P+FVector2D(28*FMath::Cos(A),28*FMath::Sin(A)),{4,5,3},I*31,21);}
}
void StringLights(FHallMesh& D,FHallMesh& G,FVector2D A,FVector2D B,int32 Bulbs)
{
    const float Z=FMath::Max(Grade(A),Grade(B))+275;for(FVector2D P:{A,B})
        D.Cylinder(Wood,At(P),FVector(P.X,P.Y,Z+10),5,7,UV);
    FVector Previous(A.X,A.Y,Z);
    for(int32 I=1;I<=Bulbs;++I)
    {const float T=float(I)/Bulbs;const FVector2D P=FMath::Lerp(A,B,T);const FVector Q(P.X,P.Y,Z-42*FMath::Sin(PI*T));
        Beam(D,Dark,Previous,Q,1);D.Cylinder(Dark,Q,Q-FVector(0,0,5),2,6,UV);
        (I%2==0?G:D).Dome(I%2==0?GlowKey:Cream,Q-FVector(0,0,8),FVector::UpVector,3,4,6);Previous=Q;}
}
void Garden(FHallMesh& S,FHallMesh& D,FVector2D P,bool Tidy)
{
    for(int32 R=0;R<3;++R)
    {const FVector2D C=P+FVector2D(R*100,0);for(float X:{C.X-38,C.X+38})GroundBox(S,Wood,{X,C.Y},{4,200,10});
        for(float Y:{C.Y-200,C.Y+200})GroundBox(S,Wood,{C.X,Y},{38,4,10});
        Paving(D,{C.X-33,C.X+33,C.Y-190,C.Y+190},Mud,45);
        if(Tidy)for(int32 K=0;K<7;++K)D.Dome(Green,At({C.X,C.Y-160+K*52},12),FVector::UpVector,20,26,7);}
}
void Swing(FHallMesh& S,FHallMesh& D,FVector2D P)
{
    const float Z=Grade(P);for(float X:{P.X-160,P.X+160})
    {Beam(S,Rust,{X-58,P.Y-90,Z},{X,P.Y,Z+245},7);Beam(S,Rust,{X+58,P.Y+90,Z},{X,P.Y,Z+245},7);}
    Beam(S,Rust,{P.X-175,P.Y,Z+245},{P.X+175,P.Y,Z+245},9);
    for(float X:{P.X-70,P.X+70})
    {for(float Q:{-22.f,22.f})Beam(D,Metal,{X+Q,P.Y,Z+238},{X+Q,P.Y+12,Z+52},1.3f);
        GroundBox(S,Dark,{X,P.Y+12},{31,16,3},0,48);}
}
void FireRing(FHallMesh& S,FHallMesh& D,FHallMesh& G,FVector2D P)
{
    // Cold charcoal and a very faint ember are static geometry. No particles, smoke or sound added.
    for(int32 I=0;I<16;++I){const float A=2*PI*I/16;GroundBox(S,Stone,P+FVector2D(95*FMath::Cos(A),95*FMath::Sin(A)),{19,14,13},I*22.5f);}
    D.Cylinder(Dark,At(P,.2f),At(P,.7f),76,18,UV);
    for(int32 I=0;I<4;++I){const float A=.65f*I;D.Cylinder(Dark,At(P+FVector2D(-58*FMath::Cos(A),-58*FMath::Sin(A)),10),At(P+FVector2D(58*FMath::Cos(A),58*FMath::Sin(A)),14),8,7,UV);}
    GroundBox(G,GlowKey,P+FVector2D(14,21),{2,3,.2f},15,1);
}
void ShoppingCart(FHallMesh& S,FHallMesh& D,FVector2D P)
{
    const FVector O=At(P);const auto W=[&](float X,float Y,float Z){return O+FVector(X,Y,Z);};
    for(float X:{-30.f,30.f})for(float Y:{-42.f,42.f})D.Cylinder(Dark,W(X,Y,9),W(X+6,Y,9),8,8,UV);
    for(float X:{-28.f,28.f})Beam(S,Metal,W(X,-45,12),W(X,40,75),3);
    Box(S,Metal,W(-31,-36,42),W(31,43,45));
    // Basket side planes are solid, while the visible open lattice retains the wire silhouette.
    for(float X:{-31.f,31.f})Box(S,Metal,W(X-1,-36,45),W(X+1,43,78));
    for(float Y=-36;Y<=43;Y+=13)for(float X:{-32.f,32.f})Beam(D,Metal,W(X,Y,45),W(X,Y,78),1);
    for(float Z:{46.f,57.f,68.f,78.f})for(float X:{-32.f,32.f})Beam(D,Metal,W(X,-36,Z),W(X,43,Z),1);
    Beam(S,Metal,W(-32,-45,83),W(32,-45,83),4);
}
void PeriodCar(FHallMesh& S,FHallMesh& D,FVector2D P)
{
    const FTransform T(FRotator(0,7,0),At(P));auto B=[&](FName K,FVector C,FVector H){S.Box(K,FTransform(T.GetRotation(),T.TransformPosition(C)),H,UV);};
    // Unbranded boxy late1970s coupe: three-box profile, inset greenhouse, chrome bumpers and hubcaps.
    B(Paint,{0,0,62},{218,91,29});B(Paint,{-12,0,113},{105,82,35});
    for(float X:{-119.f,94.f})D.Box(Dark,FTransform(T.GetRotation(),T.TransformPosition({X,0,125})),{2,75,26},UV);
    for(float Y:{-83.f,83.f})D.Box(Dark,FTransform(T.GetRotation(),T.TransformPosition({-12,Y,126})),{94,1,24},UV);
    for(float X:{-220.f,220.f})B(Metal,{X,0,51},{5,94,7});
    for(float X:{-145.f,145.f})for(float Side:{-1.f,1.f})
    {S.Cylinder(Dark,T.TransformPosition({X,Side*84,35}),T.TransformPosition({X,Side*98,35}),33,12,UV);
        D.Cylinder(Metal,T.TransformPosition({X,Side*98,35}),T.TransformPosition({X,Side*99,35}),17,10,UV);}
    for(float Y:{-67.f,67.f})D.Box(Cream,FTransform(T.GetRotation(),T.TransformPosition({223,Y,73})),{.5f,18,10},UV);
}
void Food(FHallMesh& D,FVector2D P,float Z)
{
    for(int32 I=0;I<4;++I){const FVector B=At(P+FVector2D(-80+I*48,0),Z);D.Cylinder(Cream,B,B+FVector(0,0,1),16,10,UV);
        for(int32 Q=0;Q<3;++Q)D.Dome(I%2?Sand:Red,B+FVector(-7+Q*7,0,2),FVector::UpVector,6,4,7);}
    GroundBox(D,Cream,P+FVector2D(30,21),{16,11,2},0,Z+1); // Folded paper napkins.
}
void Stereo(FHallMesh& S,FHallMesh& D,FVector2D P,float FloorZ)
{
    // Porch-mounted gear uses its architectural floor datum throughout, never the sloped ground below the deck.
    const auto W=[&](float X,float Y,float Z){return FVector(P.X+X,P.Y+Y,FloorZ+Z);};
    const auto Part=[&](FHallMesh& M,FName K,FVector C,FVector Half)
    {M.Box(K,FTransform(W(C.X,C.Y,C.Z)),Half,UV);};
    for(float Side:{-1.f,1.f})
    {Part(S,Dark,{Side*82,0,50},{27,24,50});
        for(float Z:{30.f,74.f})D.Cylinder(Metal,W(Side*82,-24,Z),W(Side*82,-25,Z),Z<40?18:8,12,UV);}
    // Narrow four-legged wood equipment table: board top75cm exactly supports the deck bottom75cm.
    Part(S,Wood,{0,0,73.5f},{49,29,1.5f});
    for(float X:{-38.f,38.f})for(float Y:{-22.f,22.f})Part(S,Wood,{X,Y,36},{2,2,36});
    for(float Y:{-22.f,22.f})Part(S,Wood,{0,Y,18},{38,1.5f,2});
    Part(S,Dark,{0,0,98},{44,25,23});
    for(int32 I=0;I<3;++I)Part(D,Metal,{-19.f+I*18,-26,87.f+I*12},{13,.4f,4});
    // Loose tape spools, cassette deck face and unplugged coiled cable. No looping audio is introduced.
    for(float X:{-12.f,12.f})D.Cylinder(Dark,W(X,-27,99),W(X,-28,99),6,8,UV);
    Beam(D,Dark,W(44,-25,81),W(50,-28,42),1.2f);Beam(D,Dark,W(50,-28,42),W(50,-28,.6f),1.2f);
    Beam(D,Dark,W(50,-28,.6f),W(48,-6,.6f),1.2f);
    for(int32 I=0;I<18;++I){const float A=I*2*PI/18,B=(I+1)*2*PI/18;
        Beam(D,Dark,W(48+6*FMath::Cos(A),6*FMath::Sin(A),.6f),W(48+6*FMath::Cos(B),6*FMath::Sin(B),.6f),1.2f);}
}
enum EKind {Cup,Can,LeafBrown,LeafYellow,Tuft,Brambles,Sapling,Paper,KindCount};
const TCHAR* BatchNames[]={TEXT("FraternityRowCups"),TEXT("FraternityRowCans"),TEXT("FraternityRowLeavesBrown"),TEXT("FraternityRowLeavesGold"),
    TEXT("FraternityRowGrass"),TEXT("FraternityRowBrambles"),TEXT("FraternityRowSaplings"),TEXT("FraternityRowPaper")};
FHallMesh Prototype(int32 Kind)
{
    FHallMesh M;
    if(Kind==Cup)
    {M.Cylinder(Red,{0,0,0},{0,0,11},3.5f,10,UV,false);M.Cylinder(Cream,{0,0,10.4f},{0,0,11},3.8f,10,UV,false);
        M.Cylinder(Dark,{0,0,9.5f},{0,0,9.6f},3.2f,10,UV);}
    else if(Kind==Can)
    {M.Cylinder(Metal,{0,0,0},{0,0,12},3.1f,10,UV);M.Cylinder(Green,{0,0,3},{0,0,8},3.15f,10,UV,false);
        Box(M,Dark,{-1,-.8f,12},{1,.8f,12.25f});}
    else if(Kind==LeafBrown||Kind==LeafYellow)
    {const FName K=Kind==LeafBrown?Leaf:LeafGold;
        M.Prism(K,{{0,-7},{-4,-2},{-3,3},{0,7},{3,3},{4,-2}},FVector::ZeroVector,{1,0,0},{0,1,.08f},FVector::UpVector,.05f,UV);
        Beam(M,Wood,{0,-9,.2f},{0,3,.4f},.25f);}
    else if(Kind==Tuft)
    {for(int32 I=0;I<9;++I){const float A=I*2.399963f,H=18+(I%4)*7;const FVector P(8*FMath::Cos(A),8*FMath::Sin(A),0),N(FMath::Cos(A),FMath::Sin(A),0),W(-N.Y,N.X,0);
        M.Quad(I%3==0?LeafGold:Grass,P-W,P+W,P+N*6+FVector(0,0,H)+W*.2f,P+N*6+FVector(0,0,H)-W*.2f,N,UV);}}
    else if(Kind==Brambles)
    {for(int32 I=0;I<4;++I){const float A=I*1.5f;const FVector Q(35*FMath::Cos(A),35*FMath::Sin(A),35+I*11);
        M.Cylinder(Wood,{0,0,0},Q,1,5,UV);for(int32 K=1;K<4;++K){const FVector P=Q*(K*.25f);M.Dome(Green,P,FVector::UpVector,12,9,5);}}}
    else if(Kind==Sapling)
    {M.Cylinder(Wood,{0,0,0},{3,1,175},2.4f,5,UV);for(int32 I=0;I<5;++I){const float A=I*2.4f;const FVector Q(34*FMath::Cos(A),34*FMath::Sin(A),85+I*16);
        M.Cylinder(Wood,{1,0,45+I*19},Q,.8f,5,UV);for(int32 J=0;J<3;++J){const FVector P=FMath::Lerp(FVector(1,0,45+I*19),Q,.5f+J*.2f);
            M.Quad(I%2?Leaf:LeafGold,P+FVector(-7,-3,0),P+FVector(7,-3,0),P+FVector(6,4,4),P+FVector(-4,4,3),FVector::UpVector,UV);}}}
    else {M.Quad(Cream,{-8,-12,0},{8,-12,0},{7,11,1},{-6,12,.6f},FVector::UpVector,UV);}
    return M;
}
void AddGround(TArray<FTransform>& Batch,FVector2D P,FRandomStream& R,float MinScale=.8f,float MaxScale=1.2f,bool Fallen=false)
{
    const float Yaw=R.FRandRange(-PI,PI),Scale=R.FRandRange(MinScale,MaxScale);
    FQuat Rotation=FQuat::FindBetweenNormals(FVector::UpVector,HouseExteriorSite::GroundNormal(P.X,P.Y))*FQuat(FVector::UpVector,Yaw);
    if(Fallen)Rotation=Rotation*FQuat(FVector::ForwardVector,HALF_PI);
    Batch.Add(FTransform(Rotation,At(P,Fallen?4.f:.6f),FVector(Scale)));
}
}

bool ExistingVegetationBlocked(const FBox2D& WorldBounds)
{
    if(!HouseFraternityRow::Enabled()||!WorldBounds.bIsValid)return false;
    auto Overlaps=[&](const FRect& R)
    {
        return WorldBounds.Max.X>R.L&&WorldBounds.Min.X<R.R
            &&WorldBounds.Max.Y>R.F&&WorldBounds.Min.Y<R.B;
    };
    for(const auto& H:HouseFraternityRow::Houses())
    {
        if(H.Index<3)continue;
        const float Front=H.Index==4?H.F-150:H.F-300,FrontSteps=Front-(H.Index==4?140:28);
        if(Overlaps({H.L,H.R,H.F,H.B})||Overlaps({H.L,H.R,Front,H.F})
            ||Overlaps({H.X()-200,H.X()+200,FrontSteps,Front}))return true;
        if(H.Index==4&&Overlaps({H.L-150,H.L,Front,H.F+550}))return true;
        // Six 28cm rear treads join the 120cm top and bottom landings.
        const float RearEnd=H.B+120+6*28+120;
        const float RearX=HouseFraternityRow::RearStairX(H);
        if(Overlaps({RearX-200,RearX+200,H.B-3,RearEnd}))return true;
    }
    for(const FRect& R:Sheds)
        if(Overlaps({R.L,R.R,R.F-15,R.B}))return true;
    return false;
}

void CreateLook(UHouseBlackHallSubsystem* Look)
{
    if(!Look||Look->Materials.Contains(Wood))return;
    HallMakePlain(Look,Wood,{.17f,.11f,.065f},.94f);HallMakePlain(Look,Paint,{.32f,.38f,.29f},.9f);
    HallMakePlain(Look,Metal,{.31f,.32f,.3f},.48f,.7f);HallMakePlain(Look,Rust,{.22f,.075f,.026f},.92f,.2f);
    HallMakePlain(Look,Cloth,{.22f,.105f,.058f},.98f);HallMakePlain(Look,Cream,{.66f,.61f,.48f},.85f);
    HallMakePlain(Look,Red,{.43f,.025f,.016f},.55f);HallMakePlain(Look,Dark,{.016f,.018f,.016f},.9f);
    HallMakePlain(Look,Stone,{.27f,.26f,.23f},.96f);HallMakePlain(Look,Mud,{.075f,.054f,.03f},.98f);
    HallMakePlain(Look,Sand,{.37f,.31f,.19f},.98f);HallMakePlain(Look,Grass,{.12f,.14f,.045f},.97f);
    HallMakePlain(Look,Green,{.045f,.08f,.024f},.97f);HallMakePlain(Look,Leaf,{.21f,.075f,.018f},.96f);
    HallMakePlain(Look,LeafGold,{.35f,.21f,.03f},.95f);HallMakePlain(Look,GlowKey,{.55f,.23f,.07f},.5f,0,{.7f,.23f,.045f});
    // Original painted sheet, using the campus's fictional opponent and its own runtime stroke font.
    FHallImage Sheet;Sheet.Init(1024,256,{.58f,.55f,.45f});
    for(int32 Y=0;Y<256;Y+=4)Sheet.Rect({0,float(Y)},{1024,float(Y+1)},{.54f,.51f,.42f},.2f);
    FHallTextStyle Text;Text.Height=112;Text.Stroke=12;Text.Spacing=.2f;Text.Wobble=2;Text.Seed=1986;
    Text.WidthScale=FMath::Min(1.f,960/HallTextWidth(TEXT("BEAT DUNMORE"),Text));
    HallDrawText(Sheet,TEXT("BEAT DUNMORE"),{(1024-HallTextWidth(TEXT("BEAT DUNMORE"),Text))*.5f,174},Text,{.29f,.018f,.012f});
    auto* Texture=HallMakeTexture(Sheet,true,false,TEXT("RY_Banner"),false);Look->Textures.Add(TEXT("RY_Banner"),Texture);
    HallMakeTextured(Look,TEXT("RY_Banner"),Texture,nullptr,.98f);
}

void Build(FHallMesh& S,FHallMesh& D,FHallMesh& G)
{
    if(!HouseFraternityRow::Enabled())return;
    const int32 SolidBefore=S.NumTriangles(),DecorBefore=D.NumTriangles(),GlowBefore=G.NumTriangles();
    Path(D,Access,180);
    // The original main-house connector atY-285..-105 is kept by integration. Wear ends before its concrete.
    for(int32 I=1;I<5;++I)Wear(D,Commons[I-1],Commons[I],180);
    Wear(D,Commons[4],{-4320,-300},180);
    Path(D,TudorApproach,160);Path(D,AnneApproach,160);
    for(const auto& P:YardPaths())for(int32 I=1;I<P.Num();++I)Wear(D,P[I-1],P[I],120);
    Divider(S,D,-15750,0,{{700,1150},{3100,3550}});Divider(S,D,-12000,1,{{700,1150},{3100,3550}});
    Divider(S,D,-8500,2,{{100,650},{3100,3600}});Divider(S,D,-6700,3,{{1050,2150},{3000,3920}});
    for(int32 I=0;I<5;++I)Shed(S,D,I);

    // R1: no party. Neglected inherited garden, rusting empty swings and stacked old chairs.
    Garden(S,D,{-18320,2600},false);Swing(S,D,{-18070,1550});
    for(int32 I=0;I<2;++I)Chair(S,D,{-16480,1050+I*90},80,true);
    GroundBox(S,Wood,{-16500,2850},{120,35,4},-8,78); // Rotting potting bench beside the rear trail.
    for(float X:{-16590.f,-16410.f})for(float Y:{2825.f,2875.f})GroundBox(S,Wood,{X,Y},{5,5,39});
    for(int32 I=0;I<4;++I)D.Cylinder(Rust,At({-16590.f+I*45,2850},86),At({-16590.f+I*45,2850},107),13,8,UV,false);

    // R2: deliberate care. Stone patio, neat vegetables, a put-away grill, raked leaf piles and a hose reel.
    Paving(D,{-15380,-14680,350,1070},Stone);Picnic(S,D,{-15030,710});Grill(S,D,{-14600,510});
    Garden(S,D,{-15180,2620},true);GroundBox(S,Wood,{-14390,2690},{45,30,46});
    D.Cylinder(Green,At({-14390,2670},58),At({-14390,2630},58),25,12,UV,false);
    for(int32 I=0;I<3;++I)D.Dome(Leaf,At({-14800.f+I*100,3460},.5f),FVector::UpVector,62,14,9);

    // R3: postgame wreckage. Full-scale sand volleyball, garage/car, dragged furniture and scattered remains.
    Paving(D,{-11550,-10650,1100,2700},Sand,150);for(float Y:{1120.f,2680.f})for(float X:{-11530.f,-10670.f})
        GroundBox(D,Cream,{X,Y},{5,10,.15f},0,.7f);
    for(float X:{-11620.f,-10580.f})Post(S,{X,1900},245,Wood);
    for(int32 I=0;I<16;++I)
    {const float A=float(I)/16,B=float(I+1)/16;const auto N=[&](float T,float H){return FVector(FMath::Lerp(-11620.f,-10580.f,T),1900,Grade({-11100,1900})+H-32*FMath::Sin(PI*T));};
        Beam(D,Cream,N(A,230),N(B,230),1.5f);Beam(D,Dark,N(A,140),N(B,140),1);
        Beam(D,Dark,N(A,140),N(A,230),.8f);for(float H:{160.f,180.f,200.f})Beam(D,Dark,N(A,H),N(B,H),.6f);}
    Paving(D,{-11500,-10600,3040,3650},Stone,100);PeriodCar(S,D,{-11050,3330});
    Couch(S,D,{-10840,450},-14,true);Chair(S,D,{-11500,650},42,true);
    FoldingTable(S,D,{-11450,307});Keg(S,D,{-11700,570});ShoppingCart(S,D,{-8850,3070});
    GroundBox(S,Cloth,{-11400,790},{100,47,8},18,1); // Abandoned striped mattress, beside the couch zone.
    for(int32 I=0;I<7;++I){const FVector2D P(-11680+I*83,-3410+I%2*60);D.Dome(Dark,At(P),FVector::UpVector,26,45,9);Beam(D,Dark,At(P,45),At(P+FVector2D(5,0),52),4);}
    // Curb remains at the original road location; no geometry is emitted into road/sidewalk.
    StringLights(D,G,{-11600,240},{-10630,400},18);
    for(FVector2D P:{FVector2D(-11800,1000),FVector2D(-11250,1000)})Post(S,P,315,Wood);
    const float BannerZ=Grade({-11525,1000});
    D.QuadUV(TEXT("RY_Banner"),{-11795,999,BannerZ+310},{-11255,999,BannerZ+310},{-11255,999,BannerZ+210},{-11795,999,BannerZ+210},
        {0,-1,0},{0,0},{1,0},{1,1},{0,1},{1,0,0});
    for(int32 I=0;I<7;++I)D.Dome(Rust,At({-11410.f+I*13,930.f+(I%3)*16},.4f),FVector::UpVector,10,7,7);
    D.Cylinder(Wood,At({-11390,949},2),At({-11372,959},8),2,6,UV); // Broken pumpkin stem among shell lobes.
    Wear(D,{-10100,900},{-10900,3000},65,Mud,.4f);Wear(D,{-9750,1250},{-9550,3000},75,Mud,.4f);

    // R4: merely messy. Picnic patio and separate commons table, one grill, a few empty chairs, half-dead bulbs.
    Paving(D,{-8310,-7790,2470,2940},Stone);Picnic(S,D,{-8050,2690});Grill(S,D,{-8160,3150});
    Chair(S,D,{-7980,3100},-18);Chair(S,D,{-8290,3350},32);Keg(S,D,{-8200,2310});
    StringLights(D,G,{-8280,2370},{-7850,3170},16);Picnic(S,D,{-8500,-850});
    // A period-unmarked cooler with its lid still askew.
    GroundBox(S,Red,{-7960,2420},{37,24,22});GroundBox(D,Cream,{-7951,2412},{39,26,2},12,45);

    // R5: stopped mid-moment. Food remains plated, stereo unplugged, lone fallen chair and cold fire ring.
    FireRing(S,D,G,{-4850,2510});Chair(S,D,{-4720,2710},-25,true);
    for(float X:{-5030.f,-4680.f})S.Cylinder(Wood,At({X,2300},20),At({X+100,2300},20),18,9,UV);
    FoldingTable(S,D,{-5950,-2450});Food(D,{-5950,-2450},77);
    const auto& Anne=HouseFraternityRow::Houses()[4];
    // Keep the west wrap accessible; the stereo occupies the east porch dead end.
    Stereo(S,D,{Anne.R-230,Anne.F-75},Anne.Floor);
    FoldingTable(S,D,{-5790,2520},10);Food(D,{-5790,2520},77);Chair(S,D,{-6000,2510},90);
    StringLights(D,G,{-6090,2370},{-5660,2450},12);
    UE_LOG(LogTemp,Display,TEXT("FRAT_ROW_YARDS authored=5 outbuildings=5 dividers=4 newLights=0 originalTreesEdited=0 terrainEdits=0 solidTriangles=%d decorTriangles=%d glowTriangles=%d"),S.NumTriangles()-SolidBefore,D.NumTriangles()-DecorBefore,G.NumTriangles()-GlowBefore);
}

void AttachInstances(AActor* Owner,UHouseBlackHallSubsystem* Look)
{
    if(!Owner||!Look||!Owner->GetRootComponent()||!HouseFraternityRow::Enabled()||Owner->GetNetMode()==NM_DedicatedServer)return;
    // Idempotent: lifetime is retained by AddInstanceComponent, meshes by each component's UPROPERTY.
    TArray<UInstancedStaticMeshComponent*> Existing;Owner->GetComponents(Existing);
    for(const auto* C:Existing)if(C&&C->GetFName()==FName(BatchNames[Cup]))return;
    CreateLook(Look);FRandomStream R(19861025);TArray<FTransform> Batches[KindCount];
    for(int32 Yard=0;Yard<5;++Yard)
    {
        const float L=Parcel[Yard]+90,Right=Parcel[Yard+1]-90;
        // A visible mown-to-wild progression. Separate seed means no shared plant/tree draws are consumed.
        for(int32 I=0;I<350;++I)
        {
            const FVector2D P(R.FRandRange(L,Right),R.FRandRange(200,4450));
            if(!Clear(P,25))continue;
            const float T=FMath::Clamp((P.Y-2450)/1750.,0.,1.);
            if(R.FRand()<.12f+T*.84f)AddGround(Batches[Tuft],P,R,.7f+T*.5f,1.f+T*1.1f);
            if(P.Y>3150&&I%13==0&&Clear(P,75))AddGround(Batches[Brambles],P,R,.7f,1.4f);
            if(P.Y>3550&&I%47==0&&Clear(P,110))AddGround(Batches[Sapling],P,R,.55f,.95f);
        }
        for(int32 I=0;I<150;++I)
        {
            const float X=(I%3==0)?R.FRandRange(L,L+160):R.FRandRange(L,Right);
            const FVector2D P(X,R.FRandRange(Yard==1?3000:400,4450));
            if(Clear(P,5))AddGround(Batches[I%2?LeafBrown:LeafYellow],P,R,.7f,1.8f);
        }
        // No party litter in R1/R2. Wrecked/moderate/interrupted use distinct counts and scatter zones.
        if(Yard<2)continue;
        const int32 Cups=Yard==2?130:Yard==3?40:18,Cans=Yard==2?70:Yard==3?20:8;
        for(int32 I=0;I<Cups+Cans+35;++I)
        {
            const FVector2D P(R.FRandRange(L,Right),R.FRandRange(Yard==2?-200:2320,Yard==2?3600:2950));
            if(!Clear(P,10))continue;
            const int32 K=I<Cups?Cup:I<Cups+Cans?Can:Paper;
            AddGround(Batches[K],P,R,.9f,1.1f,K!=Paper&&I%3!=0);
        }
    }
    // Reclaimed lot is lawn/leaf ground cover, never a parking slab. All exact old walking points remain clear.
    for(int32 I=0;I<220;++I)
    {
        const FVector2D P(R.FRandRange(-7160,-3760),R.FRandRange(-2460,-340));
        if(!Clear(P,30))continue;
        // Reserve both tables and their seating footprints; source-plan props are not runtime-derived trees.
        if(FRect{-8730,-8270,-1020,-680}.Contains(P,20)||FRect{-6100,-5800,-2550,-2350}.Contains(P,20))continue;
        AddGround(Batches[I%3==0?Tuft:I%2?LeafBrown:LeafYellow],P,R,.55f,1.15f);
    }
    // A few surface cups, not duplicated geometry: R3 pong table and the interrupted commons food table.
    for(int32 I=0;I<10;++I)Batches[Cup].Add(FTransform(FRotator::ZeroRotator,At({-11530.f+I*17,307.f+(I%2)*13},77)));
    for(int32 I=0;I<5;++I)Batches[Cup].Add(FTransform(FRotator::ZeroRotator,At({-6030.f+I*38,-2438},77)));
    int32 Total=0;
    for(int32 Kind=0;Kind<KindCount;++Kind)
    {
        if(Batches[Kind].Num()==0)continue;
        const FHallMesh MeshSource=Prototype(Kind);
        UStaticMesh* Mesh=HouseCampusStaticMesh::Build(MeshSource,FName(*FString::Printf(TEXT("SM_%s"),BatchNames[Kind])),Look);
        if(!Mesh){UE_LOG(LogTemp,Error,TEXT("FRAT_ROW_YARDS_ISM failed=%s"),BatchNames[Kind]);continue;}
        auto* C=NewObject<UInstancedStaticMeshComponent>(Owner,FName(BatchNames[Kind]),RF_Transient);
        Owner->AddInstanceComponent(C);C->SetupAttachment(Owner->GetRootComponent());C->SetMobility(EComponentMobility::Static);C->SetStaticMesh(Mesh);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetGenerateOverlapEvents(false);C->SetCanEverAffectNavigation(false);
        C->SetCastShadow(false);C->bAffectDistanceFieldLighting=false;C->bAffectDynamicIndirectLighting=false;C->SetReceivesDecals(false);
        C->SetCullDistances(Kind==Sapling?6500:3500,Kind==Sapling?9000:5500);C->RegisterComponent();C->AddInstances(Batches[Kind],false,true);
        Total+=Batches[Kind].Num();UE_LOG(LogTemp,Display,TEXT("FRAT_ROW_YARDS_ISM name=%s instances=%d triangles=%d collision=0 shadows=0 indirect=0"),BatchNames[Kind],Batches[Kind].Num(),MeshSource.NumTriangles());
    }
    UE_LOG(LogTemp,Display,TEXT("FRAT_ROW_YARDS_ISM total=%d plainISM=1 newLights=0"),Total);
}
}
