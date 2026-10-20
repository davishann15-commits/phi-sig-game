#include "HouseCalderFarmTree.h"
#include "HouseCalderFarm.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderLandform.h"
#include "HouseCalderRoadside.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderWater.h"
#include "HouseCampusLayout.h"
#include "HouseExteriorSiteLayout.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace HouseCalderFarmTreeImpl
{
const FVector2D Site(-22700, -1550);
const FName BarkKey(TEXT("CalderPersimmonBark")), FruitKey(TEXT("CalderPersimmonFruit"));
const FName LeafKey(TEXT("CalderPersimmonLeaf")), CalyxKey(TEXT("CalderPersimmonCalyx"));
double Ground(const FVector2D& P) { return HouseExteriorSite::GroundHeight(P.X, P.Y); }

bool SiteClear()
{
    // Pure site-data decision, identical before ground-cover scatter and on
    // a dedicated server. The command-line terrain configuration never varies
    // between worlds in one process.
    static const bool Valid = []()
    {
        const FBox2D Box(Site-FVector2D(300), Site+FVector2D(300));
        if (HouseCalderTerrain::BoxFrozen(Box)) return false;
        for (const HouseCampusLayout::FBlock& Block : HouseCampusLayout::Blocks())
            if (Box.Intersect(Block.Rect().ExpandBy(100))) return false;
        for (int32 Y=0; Y<5; ++Y) for (int32 X=0; X<5; ++X)
        {
            const FVector2D P(Site.X-300+X*150, Site.Y-300+Y*150);
            float Z=0;
            if (HouseCampusLayout::PavingNear(P,100) || HouseCalderFeatures::InStair(P,100)
                || HouseCalderFeatures::InRamp(P,100) || HouseCalderRoadside::Occupied(P,100)
                || HouseCalderWater::WaterAt(P,Z) || HouseCalderWater::DeckAt(P,Z)) return false;
            for (const HouseCalderLandform::FLandformPath& Path : HouseCalderLandform::LandformPaths())
                for (int32 I=0; I<(Path.bClosed?Path.Points.Num():Path.Points.Num()-1); ++I)
                {
                    const FVector2D A=Path.Points[I], D=Path.Points[(I+1)%Path.Points.Num()]-A;
                    const FVector2D Closest=A+D*FMath::Clamp(FVector2D::DotProduct(P-A,D)/FMath::Max(D.SizeSquared(),.01),0.,1.);
                    if (FVector2D::Distance(P,Closest)<Path.Width*.5+100) return false;
                }
        }
        return true;
    }();
    return Valid;
}

FVector StemControl(double Z)
{
    return FVector(Site,Ground(Site))+FVector(.045*Z+12*FMath::Sin(Z*.009), .018*Z+9*FMath::Sin(Z*.013), Z);
}
FVector Stem(double Z)
{
    const double A=Z<80?-5.:FMath::FloorToDouble(Z/80)*80., B=A<0?80.:A+80.;
    return FMath::Lerp(StemControl(A),StemControl(B),(Z-A)/(B-A));
}
double StemRadius(double Z) { return 14.-12.*FMath::Clamp(Z/640.,0.,1.); }

void Taper(FHallMesh& Mesh,FName Key,const FVector& A,const FVector& B,double RA,double RB,int32 Sides)
{
    const FVector Axis=(B-A).GetSafeNormal();
    const FVector U=FVector::CrossProduct(Axis,FMath::Abs(Axis.Z)<.9?FVector::UpVector:FVector::ForwardVector).GetSafeNormal();
    const FVector V=FVector::CrossProduct(Axis,U);
    for (int32 K=0; K<Sides; ++K)
    {
        const double T0=2*PI*K/Sides,T1=2*PI*(K+1)/Sides;
        const FVector N0=U*FMath::Cos(T0)+V*FMath::Sin(T0),N1=U*FMath::Cos(T1)+V*FMath::Sin(T1);
        const int32 First=Mesh.Sec(Key).V.Num();
        Mesh.QuadUV(Key,A+N0*RA,A+N1*RA,B+N1*RB,B+N0*RB,(N0+N1).GetSafeNormal(),
            {double(K)/Sides,A.Z/64.},{double(K+1)/Sides,A.Z/64.},{double(K+1)/Sides,A.Z/64.+(B-A).Size()/64.},{double(K)/Sides,A.Z/64.+(B-A).Size()/64.},U);
        FHallSection& Section=Mesh.Sec(Key);
        Section.N[First]=Section.N[First+3]=N0;Section.N[First+1]=Section.N[First+2]=N1;
    }
}

void MainStem(FHallMesh& Mesh)
{
    // The exact same rings generate all visual LODs and the sole collision
    // surface. Root starts under soil; no proxy box or player-only barrier.
    for (int32 K=0; K<8; ++K)
    {
        const double A=K==0?-5.:K*80.,B=(K+1)*80.;
        Taper(Mesh,BarkKey,Stem(A),Stem(B),StemRadius(A),StemRadius(B),12);
    }
}

UMaterialInterface* Painted(UObject* Outer,int32 Kind)
{
    UMaterialInterface* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured.M_NaturePropTextured"));
    if (!Parent) return nullptr;
    UMaterialInstanceDynamic* Material=UMaterialInstanceDynamic::Create(Parent,Outer);
    const int32 Size=256;FHallImage Image;Image.Init(Size,Size,FLinearColor::Black);TArray<float> Height;Height.SetNumUninitialized(Size*Size);
    const float Tau=2*PI;
    for (int32 Y=0; Y<Size; ++Y) for (int32 X=0; X<Size; ++X)
    {
        const float U=(X+.5f)/Size,V=(Y+.5f)/Size;
        const float Noise=HallFbm(U*28,V*28,3,2026101771+Kind);
        if (Kind==0)
        {
            const float Warp=.11f*FMath::Sin(Tau*V*4)+.035f*FMath::Sin(Tau*V*11);
            const float CellX=U*7+Warp+.13f*Noise;
            const float Stagger=FMath::Frac(FMath::Sin(FMath::FloorToFloat(CellX)*127.1f)*43758.5453f);
            const float CellY=V*9+.24f*FMath::Sin(Tau*U*3)+Stagger*.55f;
            const float EdgeX=FMath::Min(FMath::Frac(CellX+1),1-FMath::Frac(CellX+1));
            const float EdgeY=FMath::Min(FMath::Frac(CellY+1),1-FMath::Frac(CellY+1));
            const float Crack=1-FMath::SmoothStep(.025f,.10f,FMath::Min(EdgeX,EdgeY));
            Image.At(X,Y)=FLinearColor(.18f,.17f,.15f)*(.72f+.43f*Noise)*(1-.43f*Crack);
            Height[Y*Size+X]=Noise*.10f-Crack*.40f;
        }
        else if (Kind==1)
        {
            const float Bloom=.035f*(.5f+.5f*FMath::Sin(Tau*(U+V*2)));
            Image.At(X,Y)=FLinearColor(.53f+Bloom,.22f+Bloom,.055f+Bloom)*(.91f+.12f*Noise);
            Height[Y*Size+X]=Noise*.015f;
        }
        else if (Kind==2)
        {
            const float Rib=FMath::Exp(-FMath::Square(U-.5f)*6000);
            const float Vein=FMath::Pow(FMath::Max(0.f,FMath::Cos((V*9-FMath::Abs(U-.5f)*5)*Tau)),16.f);
            Image.At(X,Y)=FLinearColor(.30f,.20f,.055f)*(.72f+.30f*Noise)+FLinearColor(.08f,.055f,.015f)*Rib;
            Height[Y*Size+X]=Noise*.025f+Rib*.12f+Vein*.025f;
        }
        else
        {
            Image.At(X,Y)=FLinearColor(.11f,.13f,.055f)*(.8f+.25f*Noise);Height[Y*Size+X]=Noise*.06f;
        }
    }
    FHallImage Normals;HallHeightToNormal(Height,Size,Size,Kind==0?1.8f:.6f,true,Normals);
    UTexture2D* Albedo=HallMakeTexture(Image,true,false,*FString::Printf(TEXT("T_CalderPersimmon%d_A"),Kind));
    UTexture2D* Normal=HallMakeTexture(Normals,false,true,*FString::Printf(TEXT("T_CalderPersimmon%d_N"),Kind));
    if (!Albedo || !Normal) return nullptr;
    Material->SetTextureParameterValue(TEXT("BaseMap"),Albedo);Material->SetTextureParameterValue(TEXT("NormalMap"),Normal);
    Material->SetVectorParameterValue(TEXT("Tint"),FLinearColor::White);Material->SetVectorParameterValue(TEXT("Emission"),FLinearColor::Black);
    Material->SetScalarParameterValue(TEXT("Roughness"),Kind==1?.68f:.94f);
    Material->SetScalarParameterValue(TEXT("NormalStrength"),.75f);Material->SetScalarParameterValue(TEXT("UVScale"),1.f);
    return Material;
}

double BranchBaseZ(int32 I) { return 160+I*32+17*FMath::Sin(I*1.37); }
FVector BranchTip(int32 I)
{
    const double A=I*2.39996,Radius=(220-I*6)*(.78+.25*FMath::Sin(I*1.7));
    return Stem(BranchBaseZ(I))+FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,100+40*FMath::Sin(I*.8));
}
FVector BranchPoint(int32 I,double T)
{
    const FVector A=Stem(BranchBaseZ(I)),B=BranchTip(I),D=(B-A).GetSafeNormal();
    const FVector C=FMath::Lerp(A,B,.32)+FVector(-D.Y*12,D.X*12,-22);
    const FVector E=FMath::Lerp(A,B,.68)+FVector(D.Y*10,-D.X*10,9);
    return T<.32?FMath::Lerp(A,C,T/.32):T<.68?FMath::Lerp(C,E,(T-.32)/.36):FMath::Lerp(E,B,(T-.68)/.32);
}
FVector TwigBase(int32 I,int32 J) { return BranchPoint(I,.40+.18*J); }
FVector TwigTip(int32 I,int32 J)
{
    const FVector D=(BranchTip(I)-Stem(BranchBaseZ(I))).GetSafeNormal();
    const FVector Across=FVector::CrossProduct(D,FVector::UpVector).GetSafeNormal();
    return BranchPoint(I,.58+.16*J)+Across*((J%2?1:-1)*(27+J*9))+FVector(0,0,15+J*9);
}
FVector TwigMid(int32 I,int32 J) { return FMath::Lerp(TwigBase(I,J),TwigTip(I,J),.55)+FVector(0,0,-3); }
FVector FruitCentre(int32 I,int32 J) { return TwigTip(I,J)+FVector(0,0,-4.4); }

void Joint(FHallMesh& Mesh,const FVector& Centre,double Radius,int32 LOD)
{
    const int32 Sides=LOD==0?8:LOD==1?6:4,Rings=LOD==0?4:LOD==1?3:2;
    auto Point=[&](double T,double A){return Centre+FVector(FMath::Sin(T)*FMath::Cos(A),FMath::Sin(T)*FMath::Sin(A),FMath::Cos(T))*Radius;};
    for (int32 Y=0;Y<Rings;++Y) for (int32 X=0;X<Sides;++X)
    {
        const double T0=PI*Y/Rings,T1=PI*(Y+1)/Rings,A0=2*PI*X/Sides,A1=2*PI*(X+1)/Sides;
        const FVector P[]={Point(T0,A0),Point(T0,A1),Point(T1,A1),Point(T1,A0)};
        const int32 First=Mesh.Sec(BarkKey).V.Num();
        Mesh.Quad(BarkKey,P[0],P[1],P[2],P[3],(P[0]+P[1]+P[2]+P[3]-Centre*4).GetSafeNormal(),FHallUV::World(64));
        for (int32 K=0;K<4;++K) Mesh.Sec(BarkKey).N[First+K]=(P[K]-Centre).GetSafeNormal();
    }
}

void Fruit(FHallMesh& Mesh,const FVector& Centre,int32 LOD)
{
    const int32 Sides=LOD==0?10:LOD==1?7:5,Rings=LOD==0?6:LOD==1?4:3;
    const double Radius=2.05;
    auto Point=[&](double T,double A){return Centre+FVector(FMath::Sin(T)*FMath::Cos(A)*Radius,FMath::Sin(T)*FMath::Sin(A)*Radius,FMath::Cos(T)*Radius*.90);};
    for (int32 Y=0;Y<Rings;++Y) for (int32 X=0;X<Sides;++X)
    {
        const double T0=PI*Y/Rings,T1=PI*(Y+1)/Rings,A0=2*PI*X/Sides,A1=2*PI*(X+1)/Sides;
        const FVector P[]={Point(T0,A0),Point(T0,A1),Point(T1,A1),Point(T1,A0)};
        const int32 First=Mesh.Sec(FruitKey).V.Num();
        Mesh.QuadUV(FruitKey,P[0],P[1],P[2],P[3],(P[0]+P[1]+P[2]+P[3]-Centre*4).GetSafeNormal(),
            {double(X)/Sides,double(Y)/Rings},{double(X+1)/Sides,double(Y)/Rings},{double(X+1)/Sides,double(Y+1)/Rings},{double(X)/Sides,double(Y+1)/Rings},FVector::ForwardVector);
        for (int32 K=0;K<4;++K) Mesh.Sec(FruitKey).N[First+K]=(P[K]-Centre).GetSafeNormal();
    }
    const FVector Top=Centre+FVector(0,0,1.7);
    for (int32 K=0;K<4;++K)
    {
        const FVector D(FMath::Cos(K*PI*.5),FMath::Sin(K*PI*.5),0),S(-D.Y,D.X,0);
        const FVector End=Top+D*2.2+FVector(0,0,.5);
        Mesh.Quad(CalyxKey,Top,Top+D*.9+S*.7,End,Top+D*.9-S*.7,FVector::UpVector,FHallUV::World(8));
    }
    Mesh.Cylinder(BarkKey,Top+FVector(0,0,.1),Centre+FVector(0,0,4.4),.18f,LOD==0?5:3,FHallUV::World(10),false);
}

bool ClearView(const FVector& Eye,const FVector& Target)
{
    const FVector End=Target+(Eye-Target).GetSafeNormal()*8;
    auto Blocked=[&](const FVector& A,const FVector& B,double Radius)
    {
        FVector P,Q;FMath::SegmentDistToSegmentSafe(Eye,End,A,B,P,Q);
        return FVector::DistSquared(P,Q)<Radius*Radius;
    };
    for (int32 K=0;K<8;++K) if (Blocked(Stem(K*80),Stem((K+1)*80),15)) return false;
    const double T[]={0,.32,.68,1};
    for (int32 I=0;I<13;++I)
    {
        for (int32 K=0;K<3;++K) if (Blocked(BranchPoint(I,T[K]),BranchPoint(I,T[K+1]),4.5)) return false;
        for (int32 J=0;J<3;++J)
            if (Blocked(TwigBase(I,J),TwigMid(I,J),1.2) || Blocked(TwigMid(I,J),TwigTip(I,J),1.2)) return false;
    }
    return true;
}

void AutumnLeaf(FHallMesh& Mesh,const FVector& Start,double Angle,int32 LOD)
{
    const FVector D(FMath::Cos(Angle),FMath::Sin(Angle),-.18),S(-D.Y,D.X,0);
    const int32 Steps=LOD==0?4:2;
    for (int32 I=0;I<Steps;++I)
    {
        const double T0=double(I)/Steps,T1=double(I+1)/Steps;
        auto Point=[&](double T,double U){return Start+D*(T*11)+S*(U*2.3*FMath::Sin(PI*T))+FVector(0,0,1.3*FMath::Sin(PI*T)*(1-U*U));};
        for (int32 Side=0;Side<2;++Side)
        {
            const double U0=Side-1.,U1=Side;const FVector P[]={Point(T0,U0),Point(T0,U1),Point(T1,U1),Point(T1,U0)};
            const FVector N=FVector::CrossProduct(P[2]-P[0],P[3]-P[1]).GetSafeNormal();
            Mesh.QuadUV(LeafKey,P[0],P[1],P[2],P[3],N,{(U0+1)*.5,T0},{(U1+1)*.5,T0},{(U1+1)*.5,T1},{(U0+1)*.5,T1},S);
            Mesh.QuadUV(LeafKey,P[3],P[2],P[1],P[0],-N,{(U0+1)*.5,T1},{(U1+1)*.5,T1},{(U1+1)*.5,T0},{(U0+1)*.5,T0},S);
        }
    }
}
}

bool HouseCalderFarmTree::Enabled()
{
    return HouseCalderFarm::Enabled() && FParse::Param(FCommandLine::Get(),TEXT("CalderFarmTreeTrial")) && HouseCalderFarmTreeImpl::SiteClear();
}
bool HouseCalderFarmTree::Occupied(const FVector2D& Point,float Radius)
{
    return Enabled() && FVector2D::DistSquared(Point,HouseCalderFarmTreeImpl::Site)<FMath::Square(Radius+25.);
}
bool HouseCalderFarmTree::Materials(UObject* Outer,TMap<FName,UMaterialInterface*>& Out)
{
    if (!Enabled()) return true;
    namespace FI=HouseCalderFarmTreeImpl;
    const FName Keys[]={FI::BarkKey,FI::FruitKey,FI::LeafKey,FI::CalyxKey};
    for (int32 K=0;K<4;++K)
    {
        UMaterialInterface* Material=FI::Painted(Outer,K);if (!Material) return false;Out.Add(Keys[K],Material);
    }
    return true;
}
void HouseCalderFarmTree::Geometry(FHallMesh& Mesh,int32 LOD)
{
    if (!Enabled()) return;
    namespace FI=HouseCalderFarmTreeImpl;
    FI::MainStem(Mesh);
    for (int32 I=0;I<13;++I)
    {
        const double Stops[]={0,.32,.68,1},Radii[]={3.8,2.8,1.55,.45};
        for (int32 K=0;K<3;++K)
            FI::Taper(Mesh,FI::BarkKey,FI::BranchPoint(I,Stops[K]),FI::BranchPoint(I,Stops[K+1]),Radii[K],Radii[K+1],LOD==0?8:LOD==1?6:4);
        FI::Joint(Mesh,FI::BranchPoint(I,.32),2.85,LOD);
        FI::Joint(Mesh,FI::BranchPoint(I,.68),1.60,LOD);
        for (int32 J=0;J<3;++J)
        {
            const FVector End=FI::TwigTip(I,J),Base=FI::TwigBase(I,J),Mid=FI::TwigMid(I,J);
            FI::Taper(Mesh,FI::BarkKey,Base,Mid,.7,.4,LOD==0?6:LOD==1?4:3);
            FI::Taper(Mesh,FI::BarkKey,Mid,End,.4,.19,LOD==0?6:LOD==1?4:3);
            FI::Joint(Mesh,Mid,.44,LOD);
            FI::Fruit(Mesh,FI::FruitCentre(I,J),LOD);
            if (J!=1) FI::AutumnLeaf(Mesh,End,I*1.9+J,LOD);
        }
    }
}
void HouseCalderFarmTree::BuildCollision(AActor* Owner)
{
    if (!Owner || !Owner->GetWorld() || !Enabled()) return;
    namespace FI=HouseCalderFarmTreeImpl;
    FHallMesh Mesh;FI::MainStem(Mesh);const FHallSection& Stem=Mesh.Sections.FindChecked(FI::BarkKey);
    UProceduralMeshComponent* Part=NewObject<UProceduralMeshComponent>(Owner,TEXT("CalderFarmPersimmonTrunk"),RF_Transient);
    Owner->AddInstanceComponent(Part);Part->SetupAttachment(Owner->GetRootComponent());Part->SetMobility(EComponentMobility::Static);
    Part->bUseComplexAsSimpleCollision=true;Part->bUseAsyncCooking=false;Part->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    Part->SetGenerateOverlapEvents(false);Part->SetCastShadow(false);Part->bAffectDistanceFieldLighting=false;Part->bAffectDynamicIndirectLighting=false;
    Part->SetCanEverAffectNavigation(false);Part->SetVisibility(false);Part->SetHiddenInGame(true);Part->RegisterComponent();
    Part->CreateMeshSection_LinearColor(0,Stem.V,Stem.I,Stem.N,Stem.UV,TArray<FLinearColor>(),Stem.T,true);
    int32 Failures=0,Hits=0;
    for (int32 Z : {30,110,190,270,350,430,510,590}) for (int32 K=0;K<12;++K)
    {
        const FVector Centre=FI::Stem(Z),D(FMath::Cos(K*PI/6),FMath::Sin(K*PI/6),0);FHitResult Hit;
        if (!Part->LineTraceComponent(Hit,Centre+D*35,Centre,FCollisionQueryParams(SCENE_QUERY_STAT(CalderPersimmon),true))) {++Failures;continue;}
        ++Hits;
        if (FMath::Abs(FVector::Dist2D(Hit.ImpactPoint,Centre)-FI::StemRadius(Z))>1.5) ++Failures;
    }
    if (Part->GetCollisionEnabled()!=ECollisionEnabled::QueryAndPhysics || Part->CastShadow || Part->CanEverAffectNavigation()) ++Failures;
    UE_LOG(LogTemp,Display,TEXT("CALDER_PERSIMMON_TRUNK %s samples=96 hits=%d exactVisibleStem=1 branchLeafFruitCollision=0 dedicated=%d origin=(%.3f,%.3f,%.3f)"),
        Failures?TEXT("FAIL"):TEXT("PASS"),Hits,int32(Owner->GetNetMode()==NM_DedicatedServer),FI::Site.X,FI::Site.Y,FI::Ground(FI::Site));
}
bool HouseCalderFarmTree::DetailView(UWorld* World,FName Name,FVector& Eye,FVector& Target)
{
#if !UE_BUILD_SHIPPING
    if (!World || !Enabled() || (Name!=TEXT("detail_calder_persimmon") && Name!=TEXT("detail_calder_persimmon_fruit"))) return false;
    namespace FI=HouseCalderFarmTreeImpl;
    static TWeakObjectPtr<UWorld> LastWorld;static TMap<FName,TPair<FVector,FVector>> Views;
    if (LastWorld.Get()!=World) {LastWorld=World;Views.Reset();}
    if (const TPair<FVector,FVector>* Cached=Views.Find(Name)) {Eye=Cached->Key;Target=Cached->Value;return true;}
    const bool Close=Name==TEXT("detail_calder_persimmon_fruit");
    FVector Aim=Close?FI::FruitCentre(0,0):FVector(FI::Site,FI::Ground(FI::Site)+300);
    bool Found=false;
    for (int32 Candidate=0;Candidate<(Close?16:1);++Candidate)
    {
        if (Close) Aim=FI::FruitCentre(Candidate/8,0);
        int32 CameraStart=0;FParse::Value(FCommandLine::Get(),TEXT("CalderFruitCameraStart="),CameraStart);
        const double A=-PI*.5+((Candidate+FMath::Clamp(CameraStart,0,7))%8)*PI/4;
        const FVector2D P=Close?FVector2D(Aim)+FVector2D(FMath::Cos(A)*170,FMath::Sin(A)*170):FI::Site+FVector2D(-650,-550);
        const double G=FI::Ground(P);FHitResult Floor;
        if (!World->LineTraceSingleByChannel(Floor,FVector(P,G+60),FVector(P,G-90),ECC_Visibility)
            || FMath::Abs(Floor.ImpactPoint.Z-G)>4. || Floor.ImpactNormal.Z<.7) continue;
        const FVector Camera(P,Floor.ImpactPoint.Z+130);
        if (Close && !FI::ClearView(Camera,Aim)) continue;
        Eye=Camera;Target=Aim;Found=true;break;
    }
    if (!Found)
    {
        UE_LOG(LogTemp,Warning,TEXT("CALDER_PERSIMMON_CAMERA name=%s physicalOrBranchClearanceFailed=1"),*Name.ToString());
        return false;
    }
    Views.Add(Name,TPair<FVector,FVector>(Eye,Target));
    UE_LOG(LogTemp,Display,TEXT("CALDER_PERSIMMON_CAMERA name=%s groundPlus130=1 eye=%s target=%s fruitDiameterCm=4.1"),*Name.ToString(),*Eye.ToString(),*Target.ToString());
    return true;
#else
    return false;
#endif
}
