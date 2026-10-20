#include "HouseFrontPortico.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace
{
struct FColumn { FVector Center; FVector Facing; int32 Flag; };
// Four across the front and two at the rear corners, as in IMG_2418/2419/2421.
// Front centres follow the existing balcony supports, enclosing the old upper
// column geometry entirely; the room meshes and their railings stay intact.
const FColumn Columns[] = {
    {FVector(-668.11f,-1414,0), FVector(-.32f,-.948f,0),3},
    {FVector(-392.89f,-1414,0), FVector(0,-1,0),2},
    {FVector(270.35f,-1414,0), FVector(0,-1,0),1},
    {FVector(634.73f,-1414,0), FVector(.32f,-.948f,0),0},
    {FVector(-668.11f,-1097,0), FVector(-1,0,0),INDEX_NONE},
    {FVector(634.73f,-1097,0), FVector(1,0,0),INDEX_NONE}
};
constexpr int32 NU=14, NV=24;
constexpr float Hoist=104.f, Fly=168.f, ClothStep=1.f/60;
FName FlagKey(int32 K) { return FName(*FString::Printf(TEXT("FP_Flag_%d"),K)); }
FVector Mount(const FColumn& C) { return C.Center+C.Facing*59+FVector(0,0,392); }
FVector PoleTip(const FColumn& C) { return Mount(C)+C.Facing*145+FVector(0,0,166); }

void Star(FHallImage& I,FVector2D C,float R,const FLinearColor& Color)
{
    TArray<FVector2D> P;
    for(int K=0;K<10;++K)
    {
        const float A=-HALF_PI+K*PI/5, S=K%2?R*.40f:R;
        P.Add(C+FVector2D(FMath::Cos(A),FMath::Sin(A))*S);
    }
    I.FillPolygon(P,Color);
}
void PowerT(FHallImage& I,const FLinearColor& Color)
{
    I.FillPolygon({{325,175},{700,175},{700,250},{645,250},{620,225},{565,225},
        {565,415},{587,445},{630,457},{630,488},{395,488},{395,457},{438,445},
        {460,415},{460,225},{405,225},{380,250},{325,250}},Color);
}
void PaintFlag(FHallImage& I,int32 K)
{
    const FLinearColor White(.87f,.85f,.79f), Red(.63f,.025f,.035f), Blue(.016f,.04f,.15f), Orange(.95f,.265f,.018f);
    I.Init(1024,640,K==0||K==5?Orange:K==3?FLinearColor(.82f,.62f,.03f):K==4?FLinearColor(.32f,.017f,.052f):White);
    if(K==0||K==5)
    {
        if(K==5)for(int Y=0;Y<8;++Y)for(int X=0;X<12;++X)if((X+Y)%2==0)
            I.Rect(FVector2D(X*86,Y*80),FVector2D((X+1)*86,(Y+1)*80),White);
        if(K==5)I.Rect(FVector2D(267,131),FVector2D(757,531),Orange);
        PowerT(I,White);
        if(K==0)for(float Y:{545.f,577.f})I.Rect(FVector2D(0,Y),FVector2D(1024,Y+12),White);
    }
    else if(K==1)
    {
        I.Rect({0,0},{245,640},Red);I.Rect({779,0},{1024,640},Red);
        const TArray<FVector2D> Leaf={{512,121},{540,202},{580,178},{569,269},{639,227},{626,266},
            {702,257},{677,311},{700,334},{592,389},{605,426},{533,418},{526,507},{499,507},
            {505,418},{419,426},{432,389},{324,334},{347,311},{322,257},{398,266},{385,227},
            {455,269},{444,178},{484,202}};
        I.FillPolygon(Leaf,Red);
    }
    else if(K==2)
    {
        for(int Y=0;Y<13;++Y)if(Y%2==0)I.Rect(FVector2D(0,Y*640.f/13),FVector2D(1024,(Y+1)*640.f/13),Red);
        I.Rect({0,0},{430,640.f*7/13},Blue);
        for(int R=0;R<9;++R)for(int C=0;C<(R%2?5:6);++C)
            Star(I,FVector2D(36+C*71+(R%2?35:0),22+R*33),13,White);
    }
    else if(K==3)
    {
        // A coiled snake and the short caption on the yellow porch flag.
        const FLinearColor Ink(.018f,.021f,.009f);
        I.Stroke({{362,392},{340,360},{395,342},{610,344},{656,373},{619,408},{390,415},{364,395},
            {400,373},{585,374},{610,397},{591,412},{469,411},{455,383},{475,346},{505,320},
            {530,273},{519,224},{552,190},{583,204}},20,Ink);
        I.FillPolygon({{552,178},{610,195},{571,229},{549,214}},Ink);
        I.Stroke({{588,198},{624,190},{635,177}},4,Ink);
        FHallTextStyle T;T.Height=43;T.Stroke=5;T.Spacing=.20f;
        const FString Caption=TEXT("DONT TREAD ON ME");
        HallDrawText(I,Caption,FVector2D((1024-HallTextWidth(Caption,T))*.5f,548),T,Ink);
    }
    else
    {
        const FLinearColor Gold(.86f,.69f,.29f);
        I.Ellipse({512,315},{186,215},0,Gold);
        I.Ellipse({512,315},{176,204},0,FLinearColor(.32f,.017f,.052f));
        // A simple fraternity monogram on the two less-visible side supports.
        I.Ellipse({410,310},{49,77},0,Gold);I.Ellipse({410,310},{38,66},0,FLinearColor(.32f,.017f,.052f));
        I.Stroke({{410,210},{410,414}},13,Gold);
        I.Stroke({{553,236},{482,236},{530,310},{482,383},{553,383}},14,Gold);
        I.Stroke({{598,230},{598,390}},13,Gold);I.Stroke({{651,237},{600,309},{657,390}},13,Gold);
    }
    // Sewn edges and restrained fabric weave, not a shiny metal flag surface.
    for(int Y=0;Y<I.H;++Y)for(int X=0;X<I.W;++X)
    {
        const float Weave=((X%3==0?.98f:1.f)*(Y%3==0?.98f:1.f));
        I.At(X,Y)*=Weave;
        if(X<8||X>1016||Y<7||Y>632)I.At(X,Y)*=.87f;
    }
}

void CreateLook(UHouseBlackHallSubsystem* L)
{
    if(L->Materials.Contains(TEXT("FP_ColumnPaint")))return;
    FHallImage Paint;Paint.Init(512,512,FLinearColor(.83f,.825f,.79f));
    TArray<float> H;H.SetNum(512*512);
    for(int Y=0;Y<512;++Y)for(int X=0;X<512;++X)
    {
        const float N=HallNoise(X*.57f,Y*.57f,821);
        const float Wear=HallFbm(X*.045f,Y*.045f,3,522);
        Paint.At(X,Y)*=.977f+.018f*N+.016f*Wear;
        H[Y*512+X]=N*.09f;
    }
    FHallImage Normal;HallHeightToNormal(H,512,512,.10f,true,Normal);
    auto* T=HallMakeTexture(Paint,true,false,TEXT("T_FP_ColumnPaint"));
    auto* N=HallMakeTexture(Normal,false,true,TEXT("T_FP_ColumnNormal"));
    L->Textures.Add(TEXT("T_FP_ColumnPaint"),T);L->Textures.Add(TEXT("T_FP_ColumnNormal"),N);
    HallMakeTextured(L,TEXT("FP_ColumnPaint"),T,N,.62f,FLinearColor::White,.22f);
    HallMakePlain(L,TEXT("FP_BaseWear"),FLinearColor(.62f,.61f,.55f),.9f);
    HallMakePlain(L,TEXT("FP_Bracket"),FLinearColor(.61f,.62f,.61f),.28f,.8f);
    HallMakePlain(L,TEXT("FP_LanternMetal"),FLinearColor(.035f,.03f,.025f),.38f,.72f);
    HallMakePlain(L,TEXT("FP_LanternGlass"),FLinearColor(.28f,.21f,.115f),.25f,.0f,FLinearColor(.18f,.11f,.045f));
    HallMakePlain(L,TEXT("FP_BadgeEdge"),FLinearColor(.5f,.34f,.12f),.48f,.55f);
    for(int K=0;K<4;++K)
    {
        FHallImage Img;PaintFlag(Img,K);
        const FName Key=FlagKey(K), TexKey(*FString::Printf(TEXT("T_%s"),*Key.ToString()));
        auto* Tex=HallMakeTexture(Img,true,false,*TexKey.ToString(),false);
        FHallImage ClothNormal;TArray<float> Weave;Weave.SetNum(Img.W*Img.H);
        for(int Y=0;Y<Img.H;++Y)for(int X=0;X<Img.W;++X)
            Weave[Y*Img.W+X]=.12f*FMath::Sin(X*PI*.5f)*FMath::Sin(Y*PI*.5f);
        HallHeightToNormal(Weave,Img.W,Img.H,.28f,false,ClothNormal);
        const FName NormalKey(*FString::Printf(TEXT("N_%s"),*Key.ToString()));
        auto* Nrm=HallMakeTexture(ClothNormal,false,true,*NormalKey.ToString(),false);
        L->Textures.Add(TexKey,Tex);L->Textures.Add(NormalKey,Nrm);
        HallMakeTextured(L,Key,Tex,Nrm,.93f,FLinearColor::White,.28f);
    }

    FHallImage Address;Address.Init(512,128,FLinearColor(.83f,.825f,.79f));
    FHallTextStyle A;A.Height=91;A.Stroke=7;A.Spacing=.18f;
    HallDrawText(Address,TEXT("1800"),{(512-HallTextWidth(TEXT("1800"),A))*.5f,111},A,FLinearColor(.025f,.025f,.022f));
    auto* AddrTex=HallMakeTexture(Address,true,false,TEXT("T_FP_Address"),false);
    L->Textures.Add(TEXT("T_FP_Address"),AddrTex);HallMakeTextured(L,TEXT("FP_Address"),AddrTex,nullptr,.65f);

    // Painted crest on a small oval metal plate, matching the reference pediment.
    FHallImage Badge;Badge.Init(512,640,FLinearColor(.36f,.023f,.045f));
    const FLinearColor Gold(.82f,.61f,.23f),Cream(.88f,.81f,.59f);
    for(int K=0;K<48;++K)
    {
        const float Angle=K*2*PI/48;
        Badge.Stroke({FVector2D(256,260)+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*83,
            FVector2D(256,260)+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*(K%2?151:177)},3,Gold);
    }
    Badge.FillPolygon({{168,330},{344,330},{337,433},{256,492},{175,433}},Gold);
    Badge.FillPolygon({{178,340},{334,340},{327,427},{256,480},{185,427}},FLinearColor(.09f,.16f,.2f));
    Badge.Stroke({{209,349},{304,349},{304,426},{256,464},{209,426},{209,349}},3,Cream);
    Badge.Ellipse({224,260},{32,41},0,Cream);Badge.Ellipse({224,260},{24,32},0,FLinearColor(.36f,.023f,.045f));
    Badge.Stroke({{224,204},{224,314}},8,Cream);
    Badge.Stroke({{291,223},{261,223},{281,260},{261,297},{291,297}},8,Cream);
    Badge.Stroke({{317,223},{317,297}},8,Cream);Badge.Stroke({{348,223},{317,260},{348,297}},8,Cream);
    FHallTextStyle Year;Year.Height=30;Year.Stroke=2.8f;Year.Spacing=.16f;
    HallDrawText(Badge,TEXT("1873"),{(512-HallTextWidth(TEXT("1873"),Year))*.5f,532},Year,Gold);
    auto* BadgeTex=HallMakeTexture(Badge,true,false,TEXT("T_FP_Crest"),false);
    L->Textures.Add(TEXT("T_FP_Crest"),BadgeTex);HallMakeTextured(L,TEXT("FP_Crest"),BadgeTex,nullptr,.71f);
}

void TurnedColumn(FHallMesh& M,const FVector& C)
{
    // Height/radius profile: square plinth, torus base, collar, tapered shaft,
    // neck rings, rounded echinus, square abacus. Full height is 6.28 metres.
    const FVector2D Profile[]={
        {10,69},{13,72},{17,75},{22,75.5},{27,74},{32,70},{35,65},
        {36,64},{39,64},{41,60},{44,60},{47,61},{50,60},
        {115,59.5},{230,59},{345,58},{460,56.5},{568,55},
        {572,55},{574,58},{577,59},{580,58},{582,56},
        {588,56},{591,58},{594,60},{598,64},{603,69},{608,73},{613,75},{617,75}
    };
    constexpr int32 Seg=48;
    FHallSection& S=M.Sec(TEXT("FP_ColumnPaint"));const int32 Start=S.V.Num();
    for(int R=0;R<UE_ARRAY_COUNT(Profile);++R)
    {
        const FVector2D A=Profile[FMath::Max(R-1,0)],B=Profile[FMath::Min(R+1,int(UE_ARRAY_COUNT(Profile))-1)];
        const float Slope=(B.Y-A.Y)/FMath::Max(1.f,float(B.X-A.X));
        for(int K=0;K<=Seg;++K)
        {
            const float Th=K*2*PI/Seg;const FVector Rad(FMath::Cos(Th),FMath::Sin(Th),0);
            S.V.Add(C+Rad*Profile[R].Y+FVector(0,0,Profile[R].X));
            S.N.Add((Rad-FVector(0,0,Slope)).GetSafeNormal());
            S.UV.Add(FVector2D(K/float(Seg)*3,Profile[R].X/140));
            S.T.Add(FProcMeshTangent(FVector(-FMath::Sin(Th),FMath::Cos(Th),0),false));
        }
    }
    for(int R=0;R<UE_ARRAY_COUNT(Profile)-1;++R)for(int K=0;K<Seg;++K)
    {
        const int A=Start+R*(Seg+1)+K,B=A+1,C1=A+Seg+1,D=C1+1;
        S.I.Append({A,D,B,A,C1,D});
    }
    M.BoxMinMax(TEXT("FP_ColumnPaint"),C+FVector(-83,-83,-8),C+FVector(83,83,10),FHallUV::World(140));
    M.BoxMinMax(TEXT("FP_ColumnPaint"),C+FVector(-83,-83,617),C+FVector(83,83,628),FHallUV::World(140));
    // Fine seams and chips low on the bases, like the close reference photo.
    for(float Th:{.44f,2.1f,4.4f})
    {
        const FVector R(FMath::Cos(Th),FMath::Sin(Th),0);
        M.Cylinder(TEXT("FP_BaseWear"),C+R*75.7f+FVector(0,0,18),C+R*74.2f+FVector(0,0,29),.22f,4,FHallUV::World(140));
    }
}

void FrontDetails(FHallMesh& M)
{
    const FHallUV UV=FHallUV::World(140);
    // Small classical dentils and narrow bead molding on the existing fascia.
    M.BoxMinMax(TEXT("FP_ColumnPaint"),FVector(-823,-1494,629),FVector(783,-1486,633),UV);
    for(float X=-770;X<770;X+=58)
        M.BoxMinMax(TEXT("FP_ColumnPaint"),FVector(X-10,-1497,680),FVector(X+10,-1484,690),UV);
    M.QuadUV(TEXT("FP_Address"),FVector(25,-1486.5f,312),FVector(-65,-1486.5f,312),
        FVector(-65,-1486.5f,289),FVector(25,-1486.5f,289),FVector(0,-1,0),
        {0,0},{1,0},{1,1},{0,1},FVector(-1,0,0));

    const FVector C(-20,-1487,845);const float RX=70,RZ=89;
    M.Sec(TEXT("FP_BadgeEdge"));
    FHallSection& S=M.Sec(TEXT("FP_Crest"));const int32 B=S.V.Num();
    S.V.Add(C);S.N.Add(FVector(0,-1,0));S.UV.Add({.5f,.5f});S.T.Add(FProcMeshTangent(FVector(-1,0,0),false));
    for(int K=0;K<=48;++K)
    {
        const float T=K*2*PI/48;const FVector P=C+FVector(RX*FMath::Cos(T),0,RZ*FMath::Sin(T));
        S.V.Add(P);S.N.Add(FVector(0,-1,0));S.UV.Add({(.999f-FMath::Cos(T))*.5f,(1-FMath::Sin(T))*.5f});
        S.T.Add(FProcMeshTangent(FVector(-1,0,0),false));
        if(K>0)S.I.Append({B,B+K+1,B+K});
        if(K<48)
        {
            const float T1=(K+1)*2*PI/48;
            M.Cylinder(TEXT("FP_BadgeEdge"),P+FVector(0,-.6f,0),C+FVector(RX*FMath::Cos(T1),-.6f,RZ*FMath::Sin(T1)),.65f,6,UV);
        }
    }
    // Lantern cages and rain hoods use the existing three porch lights.
    auto Lantern=[&](FVector Center,bool Wall)
    {
        const FName Metal(TEXT("FP_LanternMetal")),Glass(TEXT("FP_LanternGlass"));
        M.Cylinder(Glass,Center-FVector(0,0,15),Center+FVector(0,0,15),10,8,UV);
        M.Cylinder(Metal,Center-FVector(0,0,18),Center-FVector(0,0,15),12.5f,8,UV);
        M.Cylinder(Metal,Center+FVector(0,0,15),Center+FVector(0,0,18),12.5f,8,UV);
        M.Dome(Metal,Center+FVector(0,0,18),FVector::UpVector,14,11,12);
        M.Dome(Metal,Center-FVector(0,0,18),-FVector::UpVector,12,7,12);
        for(int I=0;I<8;++I)
        {
            const float T=I*2*PI/8;const FVector R(FMath::Cos(T)*10.8f,FMath::Sin(T)*10.8f,0);
            M.Cylinder(Metal,Center+R-FVector(0,0,15),Center+R+FVector(0,0,15),.72f,6,UV);
        }
        if(Wall)
        {
            M.BoxMinMax(Metal,Center+FVector(-7,24,-14),Center+FVector(7,28,14),UV);
            M.Cylinder(Metal,Center+FVector(0,24,8),Center+FVector(0,0,30),1.7f,8,UV);
        }
        else M.Cylinder(Metal,Center+FVector(0,0,29),Center+FVector(0,0,38),1.5f,8,UV);
    };
    Lantern(FVector(-260,-1036,190),true);Lantern(FVector(190,-1036,190),true);
    Lantern(FVector(-20,-1245,258),false);
}
}

void BuildHouseFrontPortico(FHallMesh& Architecture,AActor* Owner,UHouseBlackHallSubsystem* Look)
{
    if(!Owner||!Look)return;
    CreateLook(Look);
    for(const FColumn& C:Columns)
    {
        TurnedColumn(Architecture,C.Center);
        if(C.Flag==INDEX_NONE)continue;
        const FVector P=Mount(C),Tip=PoleTip(C),Right=FVector::CrossProduct(C.Facing,FVector::UpVector);
        const FTransform Frame(FRotationMatrix::MakeFromXZ(C.Facing,FVector::UpVector).ToQuat(),P);
        Architecture.Box(TEXT("FP_Bracket"),Frame,FVector(2.4f,8,13),FHallUV::World(100));
        Architecture.Cylinder(TEXT("FP_Bracket"),P,Tip,1.75f,12,FHallUV::World(100));
        Architecture.Cylinder(TEXT("FP_Bracket"),P+FVector(0,0,-10),P+C.Facing*31+FVector(0,0,35),2.8f,10,FHallUV::World(100));
        Architecture.Dome(TEXT("FP_Bracket"),Tip,(Tip-P).GetSafeNormal(),3.5f,7,12);
        for(float Z:{-8.f,8.f})for(float X:{-5.f,5.f})
            Architecture.Cylinder(TEXT("FP_Bracket"),P+Right*X+FVector(0,0,Z)+C.Facing*2.5f,
                P+Right*X+FVector(0,0,Z)+C.Facing*3.4f,1.1f,8,FHallUV::World(100));
    }
    FrontDetails(Architecture);
    if(Owner->GetNetMode()!=NM_DedicatedServer)
    {
        auto* Flags=NewObject<UHouseFrontFlagsComponent>(Owner,TEXT("FrontColumnFlags"));
        Owner->AddInstanceComponent(Flags);Flags->RegisterComponent();Flags->Build(Look);
    }
    UE_LOG(LogTemp,Display,TEXT("FRONT_PORTICO built columns=6 front=4 rear=2 height=636 shaftDiameter=110..120 flags=4 sideFlagBrackets=0 extraLights=0"));
}

UHouseFrontFlagsComponent::UHouseFrontFlagsComponent()
{
    PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickInterval=1.f/60;
    SetIsReplicatedByDefault(false);
}
void UHouseFrontFlagsComponent::Build(UHouseBlackHallSubsystem* Look)
{
    auto* Owner=GetOwner();if(!Owner||!Look)return;
    Cloth=NewObject<UProceduralMeshComponent>(Owner,TEXT("FrontColumnFlagCloth"));
    Cloth->SetupAttachment(Owner->GetRootComponent());Cloth->SetMobility(EComponentMobility::Movable);
    Cloth->SetCollisionEnabled(ECollisionEnabled::NoCollision);Cloth->SetCastShadow(false);
    Cloth->SetReceivesDecals(false);Owner->AddInstanceComponent(Cloth);Cloth->RegisterComponent();
    const int32 Count=(NU+1)*(NV+1);
    for(const FColumn& C:Columns)
    {
        if(C.Flag==INDEX_NONE)continue;
        FFlag& F=Flags.AddDefaulted_GetRef();
        F.Along=(PoleTip(C)-Mount(C)).GetSafeNormal();F.Start=PoleTip(C)-F.Along*Hoist;
        F.Out=FVector::CrossProduct(F.Along,-FVector::UpVector).GetSafeNormal();
        F.Down=(-FVector::UpVector+F.Along*FVector::DotProduct(FVector::UpVector,F.Along)).GetSafeNormal();
        F.Phase=C.Flag*1.71f;F.MaterialIndex=C.Flag;
        F.Nodes.SetNum(Count);F.Previous.SetNum(Count);
        F.Positions.SetNum(Count*2);F.Normals.SetNum(Count*2);F.UV.SetNum(Count*2);F.Tangents.SetNum(Count*2);
        for(int J=0;J<=NV;++J)for(int I=0;I<=NU;++I)
        {
            const float U=I/float(NU),V=J/float(NV);const int32 K=J*(NU+1)+I;
            // Start gently billowed; the entire sewn hoist stays fixed to the pole.
            F.Nodes[K]=F.Start+F.Along*(Hoist*U)+(F.Down*.67f+F.Out*.742f)*(Fly*V)
                +F.Out*(5*V*FMath::Sin(U*7+V*9+F.Phase));
            F.Previous[K]=F.Nodes[K];
            F.UV[K]=F.UV[K+Count]=FVector2D(V,1-U);
        }
        auto Link=[&](int32 A,int32 B,float Length,float Stiffness){F.Links.Add({A,B,Length,Stiffness});};
        const float DU=Hoist/NU,DV=Fly/NV;
        for(int J=0;J<=NV;++J)for(int I=0;I<=NU;++I)
        {
            const int32 K=J*(NU+1)+I;
            if(I<NU)Link(K,K+1,DU,.98f);
            if(J<NV)Link(K,K+NU+1,DV,.98f);
            if(I<NU&&J<NV)
            {
                const float D=FMath::Sqrt(DU*DU+DV*DV);
                Link(K,K+NU+2,D,.55f);Link(K+1,K+NU+1,D,.55f);
            }
            // Soft bending constraints let the loose edge curl without stretching.
            if(I+2<=NU)Link(K,K+2,DU*2,.055f);
            if(J+2<=NV)Link(K,K+2*(NU+1),DV*2,.035f);
        }
    }
    SimulationTime=GetWorld()->GetTimeSeconds();
    for(int I=0;I<90;++I)Simulate(ClothStep,SimulationTime-1.5f+I*ClothStep);
    UpdateMesh();
    for(int K=0;K<Flags.Num();++K)
    {
        TArray<int32> Tri;
        for(int Side=0;Side<2;++Side)for(int J=0;J<NV;++J)for(int I=0;I<NU;++I)
        {
            const int A=Side*Count+J*(NU+1)+I,B=A+1,C=A+NU+1,D=C+1;
            if(Side==0)Tri.Append({A,D,B,A,C,D});else Tri.Append({A,B,D,A,D,C});
        }
        auto& F=Flags[K];Cloth->CreateMeshSection_LinearColor(K,F.Positions,Tri,F.Normals,F.UV,TArray<FLinearColor>(),F.Tangents,false);
        Cloth->SetMaterial(K,Look->Materials.FindRef(FlagKey(F.MaterialIndex)));
    }
}
void UHouseFrontFlagsComponent::Simulate(float Dt,float Time)
{
    for(auto& F:Flags)
    {
        const float Gust=FMath::Clamp(.52f+.27f*FMath::Sin(Time*.59f)+.19f*FMath::Sin(Time*1.27f+1.9f),.05f,1.f);
        const FVector Wind=(F.Out+FVector(0,-.21f,.12f)+FVector(0,.13f*FMath::Sin(Time*.37f),0)).GetSafeNormal();
        for(int J=1;J<=NV;++J)for(int I=0;I<=NU;++I)
        {
            const int32 K=J*(NU+1)+I;const float U=I/float(NU),V=J/float(NV);
            const FVector Old=F.Nodes[K];
            const FVector Across=F.Nodes[J*(NU+1)+FMath::Min(I+1,NU)]-F.Nodes[J*(NU+1)+FMath::Max(I-1,0)];
            const FVector Down=F.Nodes[FMath::Min(J+1,NV)*(NU+1)+I]-F.Nodes[(J-1)*(NU+1)+I];
            FVector N=FVector::CrossProduct(Across,Down).GetSafeNormal();
            if(FVector::DotProduct(N,Wind)<0)N=-N;
            const float Exposure=FMath::Abs(FVector::DotProduct(N,Wind));
            const float Ripple=FMath::Sin(V*13-U*3-Time*8.2f+F.Phase);
            const FVector Pressure=Wind*(460+Gust*900)+N*(Exposure*(250+Gust*540)+Ripple*195*V);
            F.Nodes[K]=Old+(Old-F.Previous[K])*.974f+(Pressure-FVector(0,0,720))*Dt*Dt;
            F.Previous[K]=Old;
        }
        for(int Iteration=0;Iteration<8;++Iteration)
        {
            // Reverse each pass to avoid favoring a single grid direction.
            for(int L=0;L<F.Links.Num();++L)
            {
                const auto& Link=F.Links[Iteration%2?F.Links.Num()-L-1:L];
                const FVector Delta=F.Nodes[Link.B]-F.Nodes[Link.A];const float D=Delta.Size();
                const float WA=Link.A<=NU?0.f:1.f,WB=Link.B<=NU?0.f:1.f;
                if(D<.001f||WA+WB==0)continue;
                const FVector Correction=Delta*((D-Link.Length)/D*Link.Stiffness/(WA+WB));
                F.Nodes[Link.A]+=Correction*WA;F.Nodes[Link.B]-=Correction*WB;
            }
            for(int I=0;I<=NU;++I)F.Nodes[I]=F.Start+F.Along*(Hoist*I/NU);
        }
    }
}
void UHouseFrontFlagsComponent::UpdateMesh()
{
    const int32 Count=(NU+1)*(NV+1);
    for(auto& F:Flags)for(int J=0;J<=NV;++J)for(int I=0;I<=NU;++I)
    {
        const int K=J*(NU+1)+I;
        const FVector A=F.Nodes[J*(NU+1)+FMath::Min(I+1,NU)]-F.Nodes[J*(NU+1)+FMath::Max(I-1,0)];
        const FVector B=F.Nodes[FMath::Min(J+1,NV)*(NU+1)+I]-F.Nodes[FMath::Max(J-1,0)*(NU+1)+I];
        const FVector N=FVector::CrossProduct(A,B).GetSafeNormal();
        F.Positions[K]=F.Positions[K+Count]=F.Nodes[K];
        F.Normals[K]=N;F.Normals[K+Count]=-N;
        F.Tangents[K]=FProcMeshTangent(B.GetSafeNormal(),true);
        F.Tangents[K+Count]=FProcMeshTangent(B.GetSafeNormal(),false);
    }
}
void UHouseFrontFlagsComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Dt,Type,Function);if(!Cloth||!GetWorld())return;
    // Fixed small steps keep the cloth stable even when the editor hitches.
    Accumulator+=FMath::Min(Dt,.1f);
    while(Accumulator>=ClothStep)
    {
        SimulationTime+=ClothStep;Simulate(ClothStep,SimulationTime);Accumulator-=ClothStep;
    }
    UpdateMesh();
    for(int K=0;K<Flags.Num();++K)
    {
        auto& F=Flags[K];Cloth->UpdateMeshSection_LinearColor(K,F.Positions,F.Normals,F.UV,TArray<FLinearColor>(),F.Tangents);
    }
}
