#include "HouseCalderRidgeForest.h"
#include "HouseCalderFarLand.h"
#include "HouseCalderLandscape.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"

namespace HouseCalderRidgeForestImpl
{
constexpr double TownDatum=-1900., PastPeak=TownDatum+25000.;
constexpr double XEdges[]={-320000.,-80000.,80000.,320000.};
constexpr double YEdges[]={-320000.,-120000.,-55000.,45000.,153600.};
struct FPoint { FVector Position,Normal; FLinearColor Colour; };
struct FLevel { TArray<FPoint> Points; TArray<int32> Indices; };
uint32 Hash(int32 X,int32 Y,uint32 Seed)
{
    uint32 Value=uint32(X)*374761393u+uint32(Y)*668265263u+Seed;
    Value=(Value^(Value>>13))*1274126177u;return Value^(Value>>16);
}
double Noise(double X,double Y,double Scale,uint32 Seed)
{
    const double FX=X/Scale,FY=Y/Scale;
    const int32 IX=FMath::FloorToInt(FX),IY=FMath::FloorToInt(FY);
    const double TX=FMath::SmoothStep(0.,1.,FX-IX),TY=FMath::SmoothStep(0.,1.,FY-IY);
    auto At=[Seed](int32 A,int32 B){return double(Hash(A,B,Seed)&0x00ffffffu)/16777215.;};
    return FMath::Lerp(FMath::Lerp(At(IX,IY),At(IX+1,IY),TX),FMath::Lerp(At(IX,IY+1),At(IX+1,IY+1),TX),TY);
}
double Amount(double X,double Y)
{
    double Value=FMath::Max3(FMath::SmoothStep(48000.,60000.,-Y),
        FMath::SmoothStep(43000.,50000.,Y),FMath::SmoothStep(52000.,62000.,FMath::Abs(X)));
    // Keep the town, river, road spur and the water-tower clearing open.
    const double TowerDX=FMath::Max(7200.-X,FMath::Max(X-13600.,0.));
    const double TowerDY=FMath::Max(-53600.-Y,FMath::Max(Y+47200.,0.));
    Value*=FMath::SmoothStep(0.,1800.,FMath::Sqrt(TowerDX*TowerDX+TowerDY*TowerDY));
    return Value;
}
double CrownHeight(double X,double Y,double Ground)
{
    const double High=FMath::SmoothStep(PastPeak-1800.,PastPeak+1800.,Ground);
    const double Small=Noise(X,Y,2600.,2026101723u);
    const bool Grey=High>.55 && Noise(X,Y,7000.,2026101749u)>.68;
    return 3.+Amount(X,Y)*(Grey?350.:FMath::Lerp(1450.,850.,High))*(.72+.28*Small);
}
FPoint Sample(double X,double Y)
{
    const double Ground=HouseCalderFarLand::SkylineGround(X,Y),Forest=Amount(X,Y);
    constexpr double D=1000.;
    const double GX0=HouseCalderFarLand::SkylineGround(X-D,Y),GX1=HouseCalderFarLand::SkylineGround(X+D,Y);
    const double GY0=HouseCalderFarLand::SkylineGround(X,Y-D),GY1=HouseCalderFarLand::SkylineGround(X,Y+D);
    const FVector SurfaceNormal=FVector((GX0-GX1)/(2*D),(GY0-GY1)/(2*D),1.).GetSafeNormal();
    const double High=FMath::SmoothStep(PastPeak-1800.,PastPeak+1800.,Ground);
    const double Patch=Noise(X,Y,13500.,2026101711u),Small=Noise(X,Y,2600.,2026101723u);
    const bool Grey=High>.55 && Noise(X,Y,7000.,2026101749u)>.68;
    const bool Valley=Ground-TownDatum<6500.;
    // Smoothed noise is not uniformly distributed: .20 produced only 9.16%
    // green in actual valley vertices. Its .32 quantile gives about 19.6%.
    const bool Green=Valley && Patch<.32;
    const double Concave=FMath::Clamp((GX0+GX1+GY0+GY1-4*Ground)/2000.,0.,1.);
    const bool Coves=SurfaceNormal.Y>.08 && Concave>.035 && Noise(X,Y*.2,6500.,2026101759u)>.55;
    const FLinearColor Tones[]={FLinearColor(.40f,.15f,.025f),FLinearColor(.32f,.035f,.02f),
        FLinearColor(.40f,.25f,.045f),FLinearColor(.25f,.06f,.04f),FLinearColor(.20f,.12f,.13f)};
    const double Pick=Noise(X+13000.,Y-17000.,18000.,2026101781u)*4.;
    const int32 Index=FMath::Clamp(FMath::FloorToInt(Pick),0,3);
    FLinearColor Tint=FMath::Lerp(Tones[Index],Tones[Index+1],float(Pick-Index));
    Tint=FMath::Lerp(Tint,FLinearColor(.18f,.075f,.032f),float(High));
    if(Grey)Tint=FLinearColor(.14f,.13f,.12f);
    if(Green||Coves)Tint=Coves?FLinearColor(.026f,.052f,.037f):FLinearColor(.055f,.10f,.044f);
    Tint*=float(.78+.30*Small);Tint.A=float(Forest);
    const double HX0=GX0+CrownHeight(X-D,Y,GX0),HX1=GX1+CrownHeight(X+D,Y,GX1);
    const double HY0=GY0+CrownHeight(X,Y-D,GY0),HY1=GY1+CrownHeight(X,Y+D,GY1);
    const FVector CanopyNormal=FVector((HX0-HX1)/(2*D),(HY0-HY1)/(2*D),1.).GetSafeNormal();
    return {FVector(X,Y,Ground+CrownHeight(X,Y,Ground)),CanopyNormal,Tint};
}
FLevel Grid(double X0,double X1,double Y0,double Y1,double Step)
{
    FLevel Level;
    const int32 NX=FMath::CeilToInt((X1-X0)/Step),NY=FMath::CeilToInt((Y1-Y0)/Step);
    Level.Points.Reserve((NX+1)*(NY+1));
    for(int32 I=0;I<=NX;++I)for(int32 J=0;J<=NY;++J)
        Level.Points.Add(Sample(FMath::Lerp(X0,X1,double(I)/NX),FMath::Lerp(Y0,Y1,double(J)/NY)));
    auto At=[NY](int32 I,int32 J){return I*(NY+1)+J;};
    for(int32 I=0;I<NX;++I)for(int32 J=0;J<NY;++J)
    {
        const int32 A=At(I,J),B=At(I+1,J),C=At(I+1,J+1),D=At(I,J+1);
        if(FMath::Max(FMath::Max(Level.Points[A].Colour.A,Level.Points[B].Colour.A),FMath::Max(Level.Points[C].Colour.A,Level.Points[D].Colour.A))<=0.f)continue;
        Level.Indices.Append({A,C,B,A,D,C});
    }
    return Level;
}
UStaticMesh* Mesh(const TArray<FLevel>& Levels,UMaterialInterface* Material,const FName Name)
{
    UStaticMesh* Result=NewObject<UStaticMesh>(GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UStaticMesh::StaticClass(),Name),RF_Transient);
    const bool CPU=FParse::Param(FCommandLine::Get(),TEXT("CalderRidgeForestReview"));
    Result->NeverStream=true;Result->bAllowCPUAccess=CPU;
    Result->GetStaticMaterials().Add(FStaticMaterial(Material,TEXT("Canopy")
#if WITH_EDITORONLY_DATA
        ,TEXT("Canopy")
#endif
    ));
    TArray<FMeshDescription> Descriptions;Descriptions.SetNum(3);TArray<const FMeshDescription*> Pointers;
    for(int32 LOD=0;LOD<3;++LOD)
    {
        FMeshDescription& Description=Descriptions[LOD];FStaticMeshAttributes Attributes(Description);Attributes.Register();
        auto Positions=Attributes.GetVertexPositions();auto Normals=Attributes.GetVertexInstanceNormals();
        auto Tangents=Attributes.GetVertexInstanceTangents();auto Signs=Attributes.GetVertexInstanceBinormalSigns();
        auto UV=Attributes.GetVertexInstanceUVs();UV.SetNumChannels(1);auto Colours=Attributes.GetVertexInstanceColors();
        const FPolygonGroupID Group=Description.CreatePolygonGroup();
        const FLevel& Level=Levels[LOD];TArray<int32> Vertices;Vertices.Init(INDEX_NONE,Level.Points.Num());
        for(int32 Base=0;Base<Level.Indices.Num();Base+=3)
        {
            TArray<FVertexInstanceID> Face;Face.Reserve(3);
            for(int32 K=0;K<3;++K)
            {
                const int32 Index=Level.Indices[Base+K];const FPoint& Point=Level.Points[Index];
                if(Vertices[Index]==INDEX_NONE)
                {const FVertexID Vertex=Description.CreateVertex();Positions[Vertex]=FVector3f(Point.Position);Vertices[Index]=Vertex.GetValue();}
                const FVertexInstanceID ID=Description.CreateVertexInstance(FVertexID(Vertices[Index]));Face.Add(ID);
                Normals[ID]=FVector3f(Point.Normal);Tangents[ID]=FVector3f(FVector::VectorPlaneProject(FVector::ForwardVector,Point.Normal).GetSafeNormal());
                Signs[ID]=1.f;UV.Set(ID,0,FVector2f(float(Point.Position.X/1200.),float(Point.Position.Y/1200.)));
                Colours[ID]=FVector4f(Point.Colour.R,Point.Colour.G,Point.Colour.B,Point.Colour.A);
            }
            Description.CreatePolygon(Group,Face);
        }
        Pointers.Add(&Description);
    }
    UStaticMesh::FBuildMeshDescriptionsParams Parameters;
    Parameters.bBuildSimpleCollision=false;Parameters.bFastBuild=true;Parameters.bAllowCpuAccess=CPU;
    Parameters.bMarkPackageDirty=false;Parameters.bCommitMeshDescription=false;
    UStaticMesh::FBuildMeshDescriptionsLODParams LODParameters;LODParameters.bUseFullPrecisionUVs=true;
    for(int32 LOD=0;LOD<3;++LOD)Parameters.PerLODOverrides.Add(LODParameters);
    if(!Result->BuildFromMeshDescriptions(Pointers,Parameters))return nullptr;
    if(FStaticMeshRenderData* Data=Result->GetRenderData())
    {Data->ScreenSize[0].Default=1.f;Data->ScreenSize[1].Default=.6f;Data->ScreenSize[2].Default=.2f;}
    return Result;
}
}

bool HouseCalderRidgeForest::Enabled()
{
    return HouseCalderLandscape::Enabled() && FParse::Param(FCommandLine::Get(),TEXT("CalderRidgeForestTrial"));
}
void HouseCalderRidgeForest::Build(AActor* Owner)
{
    if(!Owner||!Enabled()||!Owner->GetWorld()||Owner->GetWorld()->GetNetMode()==NM_DedicatedServer)return;
    const double Started=FPlatformTime::Seconds();
    UMaterialInterface* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderRidgeCanopy.M_CalderRidgeCanopy"));
    if(!Parent){UE_LOG(LogTemp,Error,TEXT("CALDER_RIDGE_FOREST missingMaterial=1"));return;}
    UMaterialInstanceDynamic* Material=UMaterialInstanceDynamic::Create(Parent,Owner,TEXT("CalderRidgeCanopyMaterial"));
    Material->SetTextureParameterValue(TEXT("BaseMap"),LoadObject<UTexture2D>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Textures/forest_leaves_03/T_forest_leaves_03_A.T_forest_leaves_03_A")));
    Material->SetTextureParameterValue(TEXT("NormalMap"),LoadObject<UTexture2D>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Textures/forest_leaves_03/T_forest_leaves_03_N.T_forest_leaves_03_N")));
    Material->SetScalarParameterValue(TEXT("UVScale"),1.f);Material->SetScalarParameterValue(TEXT("NormalStrength"),.35f);
    Material->SetScalarParameterValue(TEXT("Roughness"),.98f);Material->SetVectorParameterValue(TEXT("Emission"),FLinearColor::Black);
    int32 Tiles=0,Triangles=0;
    for(int32 X=0;X<3;++X)for(int32 Y=0;Y<4;++Y)
    {
        TArray<HouseCalderRidgeForestImpl::FLevel> Levels;
        for(const double Step:{2000.,4000.,8000.})Levels.Add(HouseCalderRidgeForestImpl::Grid(
            HouseCalderRidgeForestImpl::XEdges[X],HouseCalderRidgeForestImpl::XEdges[X+1],
            HouseCalderRidgeForestImpl::YEdges[Y],HouseCalderRidgeForestImpl::YEdges[Y+1],Step));
        if(Levels[0].Indices.IsEmpty())continue;
        const FName Name(*FString::Printf(TEXT("CalderRidgeCanopy_%d_%d"),X,Y));
        UStaticMesh* Canopy=HouseCalderRidgeForestImpl::Mesh(Levels,Material,FName(*FString::Printf(TEXT("SM_%s"),*Name.ToString())));
        if(!Canopy){UE_LOG(LogTemp,Error,TEXT("CALDER_RIDGE_FOREST meshFailed=%s"),*Name.ToString());continue;}
        UInstancedStaticMeshComponent* Draw=NewObject<UInstancedStaticMeshComponent>(Owner,Name);
        Draw->SetMobility(EComponentMobility::Static);Draw->SetStaticMesh(Canopy);Draw->SetForceDisableNanite(true);
        Draw->SetCollisionEnabled(ECollisionEnabled::NoCollision);Draw->SetGenerateOverlapEvents(false);Draw->SetCanEverAffectNavigation(false);
        Draw->SetCastShadow(false);Draw->bAffectDistanceFieldLighting=false;Draw->bAffectDynamicIndirectLighting=false;
        Draw->SetCullDistances(0,900000);Draw->RegisterComponent();Draw->AddInstance(FTransform::Identity);
        ++Tiles;Triangles+=Levels[0].Indices.Num()/3;
        UE_LOG(LogTemp,Display,TEXT("CALDER_RIDGE_FOREST_TILE name=%s triangles=%d/%d/%d instances=1 collision=0 shadow=0 DF=0 indirect=0"),
            *Name.ToString(),Levels[0].Indices.Num()/3,Levels[1].Indices.Num()/3,Levels[2].Indices.Num()/3);
    }
    UE_LOG(LogTemp,Display,TEXT("CALDER_RIDGE_FOREST tiles=%d triangles=%d townDatum=-1900 pastPeakAbsolute=23100 existingRidgesUntouched=1 geometryCollisionUnchanged=1 ms=%.2f trial=1"),
        Tiles,Triangles,(FPlatformTime::Seconds()-Started)*1000.);
}
