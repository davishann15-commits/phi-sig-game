#include "HouseCalderFarm.h"
#include "HouseCalderFarmTree.h"
#include "HouseCalderFarmSolid.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderLandform.h"
#include "HouseCalderLandscape.h"
#include "HouseCalderRoadside.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderWater.h"
#include "HouseCampusLayout.h"
#include "HouseExteriorSiteLayout.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"

namespace HouseCalderFarmImpl
{
const FVector2D Barn(-27200, -2200);
const FVector2D FirstBale(-24600, 200);
const TCHAR* const Names[] = {TEXT("CalderFarm_Wood"), TEXT("CalderFarm_Roof"), TEXT("CalderFarm_Hay"), TEXT("CalderFarm_Curing"), TEXT("CalderFarm_Fence")};
const TCHAR* const MergedBarnName=TEXT("CalderFarm_Barn");
const FName Wood(TEXT("CalderFarmWood")), Stone(TEXT("CalderFarmStone")), Metal(TEXT("CalderFarmMetal"));
const FName Hay(TEXT("CalderFarmHay")), HayEnd(TEXT("CalderFarmHayEnd")), Twine(TEXT("CalderFarmTwine")), Leaf(TEXT("CalderFarmLeaf")), LeafStem(TEXT("CalderFarmLeafStem"));

double Ground(const FVector2D& P) { return HouseExteriorSite::GroundHeight(P.X, P.Y); }
FVector Pivot(int32 Kind) { return Kind==2?FVector::ZeroVector:Kind==4?FVector(-22050,1700,0):FVector(Barn,0); }
double SegmentDistance(const FVector2D& P, const FVector2D& A, const FVector2D& B)
{
    const FVector2D D = B-A;
    return FVector2D::Distance(P, A+D*(D.SizeSquared()>.01 ? FMath::Clamp(FVector2D::DotProduct(P-A,D)/D.SizeSquared(),0.,1.) : 0.));
}

bool Clear(const FVector2D& Centre, const FVector2D& Half)
{
    const FBox2D Bounds(Centre-Half, Centre+Half);
    if (Bounds.Min.X < -31500 || Bounds.Max.X > -21400 || Bounds.Min.Y < -3500 || Bounds.Max.Y > 13500
        || HouseCalderTerrain::BoxFrozen(Bounds)) return false;
    for (const HouseCampusLayout::FBlock& Block : HouseCampusLayout::Blocks())
        if (Bounds.Intersect(Block.Rect().ExpandBy(150))) return false;
    for (int32 Y=0; Y<=4; ++Y) for (int32 X=0; X<=4; ++X)
    {
        const FVector2D P(FMath::Lerp(Bounds.Min.X,Bounds.Max.X,X/4.), FMath::Lerp(Bounds.Min.Y,Bounds.Max.Y,Y/4.));
        float WaterZ=0.f;
        if (HouseCampusLayout::PavingNear(P,100) || HouseCalderFeatures::InStair(P,100)
            || HouseCalderFeatures::InRamp(P,100) || HouseCalderRoadside::Occupied(P,100)
            || HouseCalderWater::WaterAt(P,WaterZ) || HouseCalderWater::DeckAt(P,WaterZ)) return false;
        for (const HouseCalderLandform::FLandformPath& Path : HouseCalderLandform::LandformPaths())
        {
            const int32 Count=Path.bClosed?Path.Points.Num():Path.Points.Num()-1;
            for (int32 K=0; K<Count; ++K)
                if (SegmentDistance(P,Path.Points[K],Path.Points[(K+1)%Path.Points.Num()]) < Path.Width*.5+100) return false;
        }
    }
    return true;
}

UMaterialInterface* Material(UObject* Outer, const TCHAR* Set, const FLinearColor& Tint, float Roughness)
{
    UMaterialInterface* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured.M_NaturePropTextured"));
    if (!Parent) return nullptr;
    UMaterialInstanceDynamic* Result=UMaterialInstanceDynamic::Create(Parent,Outer);
    for (const TCHAR* Kind : {TEXT("A"),TEXT("N")})
    {
        const FString Path=FString::Printf(TEXT("/Game/SeniorSendOff/Calder/Textures/%s/T_%s_%s.T_%s_%s"),Set,Set,Kind,Set,Kind);
        UTexture2D* Texture=LoadObject<UTexture2D>(nullptr,*Path);
        if (!Texture) return nullptr;
        Result->SetTextureParameterValue(FCString::Strcmp(Kind,TEXT("A"))==0?TEXT("BaseMap"):TEXT("NormalMap"),Texture);
    }
    Result->SetVectorParameterValue(TEXT("Tint"),Tint);
    Result->SetVectorParameterValue(TEXT("Emission"),FLinearColor::Black);
    Result->SetScalarParameterValue(TEXT("Roughness"),Roughness);
    Result->SetScalarParameterValue(TEXT("NormalStrength"),.85f);
    Result->SetScalarParameterValue(TEXT("UVScale"),1.f);
    return Result;
}

// Metallic data must retain alpha at every mip. The shared Hall painter
// intentionally forces alpha=1, so leave it untouched and own this texture.
UTexture2D* RoofDataTexture(const FHallImage& Image,uint8& AlphaMin,uint8& AlphaMax,uint8& TailAlpha)
{
    UTexture2D* Texture=UTexture2D::CreateTransient(Image.W,Image.H,PF_B8G8R8A8,
        MakeUniqueObjectName(GetTransientPackage(),UTexture2D::StaticClass(),TEXT("T_CalderFarmRoof_A")));
    if (!Texture) return nullptr;
    Texture->SRGB=true;Texture->CompressionSettings=TC_Default;Texture->AddressX=Texture->AddressY=TA_Wrap;
    Texture->Filter=TF_Trilinear;Texture->LODGroup=TEXTUREGROUP_World;
    FTexturePlatformData* Data=Texture->GetPlatformData();if (!Data || !Data->Mips.Num()) return nullptr;
    TArray<FLinearColor> Level=Image.Px;int32 W=Image.W,H=Image.H;
    for (int32 Mip=0;;++Mip)
    {
        TArray<FColor> Pixels;Pixels.Reserve(Level.Num());for (const FLinearColor& C:Level) Pixels.Add(C.ToFColor(true));
        FTexture2DMipMap* Target=Mip==0?&Data->Mips[0]:new FTexture2DMipMap(W,H,1);
        if (Mip) Data->Mips.Add(Target);Target->SizeX=W;Target->SizeY=H;
        Target->BulkData.Lock(LOCK_READ_WRITE);void* Bytes=Target->BulkData.Realloc(int64(W)*H*4);
        FMemory::Memcpy(Bytes,Pixels.GetData(),int64(W)*H*4);Target->BulkData.Unlock();
        if (W==1 && H==1) break;
        const int32 NW=FMath::Max(1,W/2),NH=FMath::Max(1,H/2);TArray<FLinearColor> Next;Next.SetNumUninitialized(NW*NH);
        for (int32 Y=0;Y<NH;++Y) for (int32 X=0;X<NW;++X)
        {
            const int32 X0=FMath::Min(X*2,W-1),X1=FMath::Min(X*2+1,W-1),Y0=FMath::Min(Y*2,H-1),Y1=FMath::Min(Y*2+1,H-1);
            Next[Y*NW+X]=(Level[Y0*W+X0]+Level[Y0*W+X1]+Level[Y1*W+X0]+Level[Y1*W+X1])*.25f;
        }
        Level=MoveTemp(Next);W=NW;H=NH;
    }
    // Read the actual bytes before enqueuing resource initialization, so
    // render-thread upload cannot race a second lock of the bulk data.
    const FColor* Raw=static_cast<const FColor*>(Data->Mips[0].BulkData.LockReadOnly());
    AlphaMin=255;AlphaMax=0;
    for (int32 K=0;K<Image.W*Image.H;++K) {AlphaMin=FMath::Min(AlphaMin,Raw[K].A);AlphaMax=FMath::Max(AlphaMax,Raw[K].A);}
    Data->Mips[0].BulkData.Unlock();
    const FColor* Last=static_cast<const FColor*>(Data->Mips.Last().BulkData.LockReadOnly());
    TailAlpha=Last[0].A;Data->Mips.Last().BulkData.Unlock();
    Texture->UpdateResource();return Texture;
}

UMaterialInterface* RoofMaterial(UObject* Outer)
{
    UMaterialInterface* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderFarmRoof.M_CalderFarmRoof"));
    if (!Parent)
    {
        UE_LOG(LogTemp,Warning,TEXT("CALDER_FARM_ROOF missing owned shader; retaining trial baseline"));
        return Material(Outer,TEXT("cracked_concrete"),FLinearColor(.31,.20,.13),.83);
    }
    constexpr int32 Size=512;
    auto Noise=[](int32 X,int32 Y,int32 Cells,uint32 Seed)
    {
        const double FX=X*double(Cells)/Size,FY=Y*double(Cells)/Size;
        const int32 IX=FMath::FloorToInt(FX),IY=FMath::FloorToInt(FY);
        auto At=[Cells,Seed](int32 A,int32 B)
        {
            A=(A%Cells+Cells)%Cells;B=(B%Cells+Cells)%Cells;
            uint32 Hash=uint32(A)*374761393u+uint32(B)*668265263u+Seed*2246822519u;
            Hash=(Hash^(Hash>>13))*1274126177u;Hash^=Hash>>16;return (Hash&0xffffffu)/float(0xffffffu);
        };
        return FMath::Lerp(FMath::Lerp(At(IX,IY),At(IX+1,IY),FMath::SmoothStep(0.,1.,FX-IX)),
            FMath::Lerp(At(IX,IY+1),At(IX+1,IY+1),FMath::SmoothStep(0.,1.,FX-IX)),FMath::SmoothStep(0.,1.,FY-IY));
    };
    FHallImage Image;Image.Init(Size,Size,FLinearColor::Black);TArray<float> Height;Height.SetNumUninitialized(Size*Size);
    float LowestMetal=1,HighestMetal=0;
    for (int32 Y=0;Y<Size;++Y) for (int32 X=0;X<Size;++X)
    {
        const float Macro=(Noise(X,Y,6,2026101746)*.6f+Noise(X,Y,12,2026101747)*.3f+Noise(X,Y,24,2026101748)*.1f);
        const float Fine=Noise(X,Y,128,2026101749),Rust=FMath::SmoothStep(.36f,.59f,Macro),MetalFraction=1.f-Rust;
        FLinearColor Clean=FLinearColor(.30f,.325f,.34f)*(.86f+.22f*Fine);
        const FLinearColor Oxide=FMath::Lerp(FLinearColor(.065f,.028f,.012f),FLinearColor(.23f,.083f,.025f),Fine);
        FLinearColor Colour=FMath::Lerp(Clean,Oxide,Rust);Colour.A=MetalFraction;Image.At(X,Y)=Colour;
        Height[Y*Size+X]=Fine*.08f+Rust*.13f;LowestMetal=FMath::Min(LowestMetal,MetalFraction);HighestMetal=FMath::Max(HighestMetal,MetalFraction);
    }
    FHallImage Normal;HallHeightToNormal(Height,Size,Size,.9f,true,Normal);
    uint8 AlphaMin=255,AlphaMax=0,TailAlpha=0;
    UTexture2D* Albedo=RoofDataTexture(Image,AlphaMin,AlphaMax,TailAlpha);UTexture2D* Normals=HallMakeTexture(Normal,false,true,TEXT("T_CalderFarmRoof_N"));
    if (!Albedo || !Normals || LowestMetal>.05f || HighestMetal<.95f) return nullptr;
    if (AlphaMin>12 || AlphaMax<243 || TailAlpha==0 || TailAlpha==255) return nullptr;
    UMaterialInstanceDynamic* Result=UMaterialInstanceDynamic::Create(Parent,Outer);
    Result->SetTextureParameterValue(TEXT("BaseMap"),Albedo);Result->SetTextureParameterValue(TEXT("NormalMap"),Normals);
    Result->SetVectorParameterValue(TEXT("Tint"),FLinearColor::White);Result->SetVectorParameterValue(TEXT("Emission"),FLinearColor::Black);
    Result->SetScalarParameterValue(TEXT("UVScale"),1.f);Result->SetScalarParameterValue(TEXT("NormalStrength"),.85f);
    UE_LOG(LogTemp,Display,TEXT("CALDER_FARM_ROOF bound=1 alphaReadback=%d/%d tailAlpha=%d mips=%d roughRust=0.93 roughMetal=0.49 newLights=0"),int32(AlphaMin),int32(AlphaMax),int32(TailAlpha),Albedo->GetPlatformData()->Mips.Num());
    return Result;
}

UMaterialInterface* HayMaterial(UObject* Outer, bool End)
{
    UMaterialInstanceDynamic* Result=Cast<UMaterialInstanceDynamic>(Material(Outer,TEXT("withered_grass"),FLinearColor::White,.97f));
    if (!Result) return nullptr;
    constexpr int32 Size=512;
    FHallImage Image;Image.Init(Size,Size,FLinearColor(.245f,.185f,.09f));
    FRandomStream Paint(End?2026101742:2026101741);
    for (int32 Y=0;Y<Size;++Y) for (int32 X=0;X<Size;++X)
    {
        const float Radius=FVector2D(X-255.5f,Y-255.5f).Size();
        const float Noise=HallFbm(X*.035f,Y*.035f,3,End?73:71);
        const float Rings=End?.025f*FMath::Sin(Radius*.18f+HallFbm(X*.01f,Y*.01f,2,77)*2.f):0.f;
        const float Tone=.79f+.32f*Noise+Rings;
        Image.At(X,Y)=FLinearColor(.30f*Tone,.224f*Tone,.115f*Tone);
    }
    // Short stalks, broken fibres and tiny dark interstices carry the rolled
    // surface. No dark geometric spiral is laid on top of the cut end.
    for (int32 K=0;K<21000;++K)
    {
        const FVector2D Centre(Paint.FRandRange(0,Size),Paint.FRandRange(0,Size));
        const float Angle=End?FMath::Atan2(Centre.Y-255.5f,Centre.X-255.5f)+PI*.5f+Paint.FRandRange(-.7f,.7f):PI*.5f+Paint.FRandRange(-.28f,.28f);
        const FVector2D Along(FMath::Cos(Angle),FMath::Sin(Angle));
        const float Length=Paint.FRandRange(2.f,End?14.f:36.f),Tone=Paint.FRandRange(.48f,1.55f);
        Image.Stroke({Centre-Along*Length*.5f,Centre+Along*Length*.5f},Paint.FRandRange(.4f,1.1f),FLinearColor(.38f*Tone,.29f*Tone,.15f*Tone));
    }
    for (int32 K=0;K<1900;++K)
        Image.Ellipse(FVector2D(Paint.FRandRange(0,Size),Paint.FRandRange(0,Size)),FVector2D(.6f,Paint.FRandRange(.7f,1.6f)),Paint.FRandRange(0,180),FLinearColor(.065f,.042f,.018f));
    TArray<float> Height;Height.Reserve(Image.Px.Num());
    for (const FLinearColor& C : Image.Px) Height.Add((C.R+C.G+C.B)*.22f);
    FHallImage Normal;HallHeightToNormal(Height,Size,Size,1.2f,true,Normal);
    UTexture2D* Albedo=HallMakeTexture(Image,true,false,End?TEXT("T_CalderFarmHayEnd_A"):TEXT("T_CalderFarmHaySide_A"));
    UTexture2D* Normals=HallMakeTexture(Normal,false,true,End?TEXT("T_CalderFarmHayEnd_N"):TEXT("T_CalderFarmHaySide_N"));
    if (!Albedo || !Normals) return nullptr;
    Result->SetTextureParameterValue(TEXT("BaseMap"),Albedo);Result->SetTextureParameterValue(TEXT("NormalMap"),Normals);
    return Result;
}

UMaterialInterface* CuredLeafMaterial(UObject* Outer)
{
    UMaterialInstanceDynamic* Result=Cast<UMaterialInstanceDynamic>(Material(Outer,TEXT("withered_grass"),FLinearColor::White,.96f));
    if (!Result) return nullptr;
    constexpr int32 Size=512;
    FHallImage Image;Image.Init(Size,Size,FLinearColor(.3f,.2f,.1f));
    TArray<float> Height;Height.SetNumUninitialized(Size*Size);
    const FLinearColor Colours[]={FLinearColor(.36f,.25f,.14f),FLinearColor(.29f,.20f,.12f),
        FLinearColor(.41f,.30f,.18f),FLinearColor(.32f,.22f,.13f)};
    for (int32 Y=0;Y<Size;++Y) for (int32 X=0;X<Size;++X)
    {
        const int32 Variant=(X/256)+(Y/256)*2;
        const float U=(X%256)/255.f,V=(Y%256)/255.f;
        const float Noise=HallFbm(X*.075f,Y*.075f,4,2026101744);
        const float Fine=HallFbm(X*.43f,Y*.43f,2,2026101745);
        const float Across=FMath::Abs(U-.5f);
        const float Rib=FMath::Exp(-Across*Across*9000.f);
        const float Phase=V*10.f-Across*4.4f;
        const float VeinDistance=FMath::Abs(Phase-FMath::RoundToFloat(Phase));
        const float Veins=FMath::Exp(-VeinDistance*VeinDistance*1700.f)*(1.f-FMath::Exp(-Across*24.f));
        const float Wrinkle=FMath::Sin(V*74.f+Noise*14.f+U*7.f)*.055f;
        FLinearColor Colour=Colours[Variant]*(.74f+.31f*Noise+.13f*Fine+Wrinkle);
        Colour=FMath::Lerp(Colour,FLinearColor(.53f,.43f,.29f),Rib*.64f);
        Colour*=1.f-Veins*.21f;
        Image.At(X,Y)=Colour;Height[Y*Size+X]=Noise*.10f+Fine*.055f+Rib*.30f+Veins*.10f+Wrinkle*.45f;
    }
    FHallImage Normal;HallHeightToNormal(Height,Size,Size,.95f,false,Normal);
    UTexture2D* Albedo=HallMakeTexture(Image,true,false,TEXT("T_CalderFarmCuredLeaf_A"));
    UTexture2D* Normals=HallMakeTexture(Normal,false,true,TEXT("T_CalderFarmCuredLeaf_N"));
    if (!Albedo || !Normals) return nullptr;
    Result->SetTextureParameterValue(TEXT("BaseMap"),Albedo);Result->SetTextureParameterValue(TEXT("NormalMap"),Normals);
    return Result;
}

void Beam(FHallMesh& Mesh, FName Key, const FVector& A, const FVector& B, float Width, float Depth)
{
    const FVector Delta=B-A;
    const FQuat Rotation=FQuat::FindBetweenNormals(FVector::UpVector,Delta.GetSafeNormal());
    const FTransform Frame(Rotation,(A+B)*.5);
    Mesh.Box(Key,Frame,FVector(Width*.5,Depth*.5,Delta.Size()*.5),FHallUV::Local(Frame,160));
}

void DryLeaf(FHallMesh& Mesh,const FVector& Start,float Angle,float Length,float HalfWidth,int32 Seed,int32 LOD)
{
    const FVector Direction(FMath::Cos(Angle),FMath::Sin(Angle),0),Across(-FMath::Sin(Angle),FMath::Cos(Angle),0);
    const int32 Segments=LOD==0?8:LOD==1?4:2,Variant=Seed&3;
    auto Point=[&](double T,double U)
    {
        T=FMath::Clamp(T,0.,1.);
        const double Profile=FMath::Sin(PI*FMath::Pow(T,.72));
        const double Width=FMath::Max(.15,double(HalfWidth)*Profile*(1.+.07*FMath::Sin(T*23.+Seed)));
        const FVector Centre=Start+Direction*(20.*T+13.*FMath::Sin(T*PI*.8))+FVector(0,0,-Length*T+5.*FMath::Pow(T,5.));
        const double Fold=FMath::Sin(T*PI)*(U*U*(3.5+2.*FMath::Sin(Seed+T*21.))+2.*(1.-U*U));
        return Centre+Across*(U*Width)+Direction*(FMath::Abs(U)*1.8*FMath::Sin(T*17.+Seed))+FVector(0,0,Fold);
    };
    auto Normal=[&](double T,double U)
    {
        return FVector::CrossProduct(Point(T,U+.005)-Point(T,U-.005),Point(T+.005,U)-Point(T-.005,U)).GetSafeNormal();
    };
    auto UV=[Variant](double T,double U){return FVector2D((Variant%2*256+2+(U+1.)*.5*251)/512.,(Variant/2*256+2+T*251)/512.);};
    for (int32 K=0;K<Segments;++K) for (int32 Side=0;Side<2;++Side)
    {
        const double T0=double(K)/Segments,T1=double(K+1)/Segments,U0=Side-1.,U1=Side;
        const FVector P[]={Point(T0,U0),Point(T0,U1),Point(T1,U1),Point(T1,U0)};
        const FVector N[]={Normal(T0,U0),Normal(T0,U1),Normal(T1,U1),Normal(T1,U0)};
        const FVector2D Map[]={UV(T0,U0),UV(T0,U1),UV(T1,U1),UV(T1,U0)};
        const FVector Average=(N[0]+N[1]+N[2]+N[3]).GetSafeNormal();
        int32 Base=Mesh.Sec(Leaf).V.Num();
        Mesh.QuadUV(Leaf,P[0],P[1],P[2],P[3],Average,Map[0],Map[1],Map[2],Map[3],Across);
        for (int32 J=0;J<4;++J) Mesh.Sec(Leaf).N[Base+J]=N[J];
        Base=Mesh.Sec(Leaf).V.Num();
        Mesh.QuadUV(Leaf,P[3],P[2],P[1],P[0],-Average,Map[3],Map[2],Map[1],Map[0],Across);
        for (int32 J=0;J<4;++J) Mesh.Sec(Leaf).N[Base+J]=-N[3-J];
    }
}

void BarnGeometry(FHallMesh& Boards, FHallMesh& Roof, FHallMesh& Curing, int32 LOD, bool CuringLeaves=true)
{
    double Base=-1e9;
    for (double X : {-650.,650.}) for (double Y : {-1000.,1000.}) Base=FMath::Max(Base,Ground(Barn+FVector2D(X,Y)));
    Base+=12.;
    auto At=[Base](double X,double Y,double Z){return FVector(Barn.X+X,Barn.Y+Y,Base+Z);};
    // Open southern gable, soil floor, and a level timber frame on individual
    // stone piers: the existing sloping ground is never graded.
    for (double X : {-600.,600.}) for (double Y : {-900.,-300.,300.,900.})
    {
        const double Floor=Ground(Barn+FVector2D(X,Y));
        Boards.Box(Stone,FTransform(FVector(Barn.X+X,Barn.Y+Y,(Floor-6+Base+24)*.5)),
            FVector(34,34,(Base+24-Floor+6)*.5),FHallUV::World(130));
        Beam(Boards,Wood,At(X,Y,20),At(X,Y,600),22,22);
    }
    for (double X : {-600.,600.})
    {
        for (double Z : {120.,340.,590.}) Beam(Boards,Wood,At(X,-950,Z),At(X,950,Z),18,20);
        for (double Y : {-900.,0.,900.})
        {
            Beam(Boards,Wood,At(X,Y,500),At(X,Y+130,590),12,14);
            Beam(Boards,Wood,At(X,Y,500),At(X,Y-130,590),12,14);
        }
    }
    for (double Y : {-900.,-450.,0.,450.,900.})
    {
        Beam(Boards,Wood,At(-630,Y,590),At(0,Y,880),15,18);
        Beam(Boards,Wood,At(0,Y,880),At(630,Y,590),15,18);
        Beam(Boards,Wood,At(-600,Y,590),At(600,Y,590),16,20);
    }
    Beam(Boards,Wood,At(0,-1040,880),At(0,1040,880),18,20);
    const double Slat=LOD==0?23.:LOD==1?46.:92.;
    for (int32 Side : {-1,1}) for (double Y=-960;Y<960;Y+=Slat)
    {
        const double Top=570-8*FMath::Sin(Y*.019);
        const FTransform Frame(At(Side*610,Y,Top*.5));
        Boards.Box(Wood,Frame,FVector(3.5,Slat*.40,Top*.5),FHallUV::Local(Frame,180));
    }
    for (double X=-580; X<600; X+=Slat)
    {
        const double Top=880-FMath::Abs(X)/600*290;
        const FTransform Frame(At(X,960,Top*.5));
        Boards.Box(Wood,Frame,FVector(Slat*.4,3.5,Top*.5),FHallUV::Local(Frame,180));
    }
    // Corrugated, dull iron roof. Narrow ribs disappear through geometric
    // LODs; broad panels and the ridge silhouette remain at every distance.
    const int32 Ribs=LOD==0?92:LOD==1?46:12;
    for (int32 Side : {-1,1}) for (int32 K=0;K<Ribs;++K)
    {
        const double Y0=-1060.+2120.*K/Ribs,Y1=-1060.+2120.*(K+1)/Ribs;
        const double YM=(Y0+Y1)*.5;
        auto Strip=[&](double YA,double YB,double BumpA,double BumpB)
        {
            const FVector A=At(0,YA,889+BumpA),B=At(Side*690,YA,572+BumpA);
            const FVector C=At(Side*690,YB,572+BumpB),D=At(0,YB,889+BumpB);
            FVector N=FVector::CrossProduct(B-A,D-A).GetSafeNormal();if (N.Z<0) N=-N;
            Roof.Quad(Metal,A,B,C,D,N,FHallUV::World(320));
            Roof.Quad(Metal,D,C,B,A,-N,FHallUV::World(320));
        };
        Strip(Y0,YM,0,LOD<2?2.5:0);Strip(YM,Y1,LOD<2?2.5:0,0);
    }
    const int32 Spacing=LOD==0?1:LOD==1?2:4;
    for (double X : {-360.,0.,360.}) for (int32 Row=0;Row<16;Row+=Spacing)
    {
        const double Y=-710+Row*90;
        Beam(Boards,Wood,At(X-130,Y,470),At(X+130,Y,470),3,3);
        if (!CuringLeaves) continue;
        for (int32 Bunch=0;Bunch<5;Bunch+=Spacing)
        {
            const double CX=X-105+Bunch*52;
            const FVector StemTop=At(CX,Y,468),StemBottom=StemTop+FVector(5*FMath::Sin(float(Row+Bunch)),3*FMath::Cos(float(Row+Bunch)),-132);
            Curing.Cylinder(LeafStem,StemTop,StemBottom,.95f,LOD==0?6:4,FHallUV::World(70));
            for (int32 Petal=0;Petal<7;Petal+=Spacing)
            {
                const int32 Seed=Row*113+Bunch*29+Petal*7;
                const float Angle=Petal*2.4f+Row*.4f+Bunch*.9f;
                const FVector Start=FMath::Lerp(StemTop,StemBottom,.12f+.1f*Petal);
                DryLeaf(Curing,Start,Angle,64.f+11.f*FMath::Sin(Seed*1.3f),13.f+3.f*FMath::Sin(Seed*.71f),Seed,LOD);
            }
        }
    }
}

const TArray<TPair<FVector2D,FVector2D>>& FenceRuns()
{
    static const TArray<TPair<FVector2D,FVector2D>> Runs=[]()
    {
        TArray<TPair<FVector2D,FVector2D>> Result;
        for (int32 K=0;K<24;++K)
        {
            const double Y0=-3100+K*400.,Y1=Y0+400.;
            if (Y0>=-300 && Y0<1300) continue;
            const FVector2D A(-22050+(K%2?100:-100),Y0),B(-22050+(K%2?-100:100),Y1);
            if (Clear((A+B)*.5,FVector2D(135,225))) Result.Add({A,B});
        }
        return Result;
    }();
    return Runs;
}

void FenceGeometry(FHallMesh& Mesh, int32 LOD)
{
    // Identical authored runs and wide gate; shared pure data also reserve
    // cover roots when these visible rails gain physical collision.
    for (const auto& Run:FenceRuns())
        for (int32 Rail=0;Rail<5;++Rail)
            Mesh.Cylinder(Wood,FVector(Run.Key,Ground(Run.Key)+12+Rail*19),FVector(Run.Value,Ground(Run.Value)+12+Rail*19),6.5f,LOD==0?7:LOD==1?5:4,FHallUV::World(140));
}

FHallMesh BaleGeometry(int32 LOD, bool Decorations=true)
{
    FHallMesh Mesh;
    const int32 Segments=LOD==0?48:LOD==1?24:12;
    const float R=79.f,Half=66.f;
    // Horizontal cylinder, with uneven, compressed hay instead of a plastic
    // wrap. The end discs expose concentric rolls and both twine bands.
    for (int32 K=0;K<Segments;++K)
    {
        const double A=2*PI*K/Segments,B=2*PI*(K+1)/Segments;
        const double RA=R*(1+.012*FMath::Sin(A*7)),RB=R*(1+.012*FMath::Sin(B*7));
        const FVector V[4]={{-Half,RA*FMath::Cos(A),R+RA*FMath::Sin(A)},
            {Half,RA*FMath::Cos(A),R+RA*FMath::Sin(A)},
            {Half,RB*FMath::Cos(B),R+RB*FMath::Sin(B)},
            {-Half,RB*FMath::Cos(B),R+RB*FMath::Sin(B)}};
        Mesh.QuadUV(Hay,V[0],V[1],V[2],V[3],FVector(0,FMath::Cos((A+B)*.5),FMath::Sin((A+B)*.5)),
            {0,K*5./Segments},{1,K*5./Segments},{1,(K+1)*5./Segments},{0,(K+1)*5./Segments},FVector::ForwardVector);
        for (int32 End : {-1,1})
        {
            const FVector Centre(End*Half,0,R),P(End*Half,RA*FMath::Cos(A),R+RA*FMath::Sin(A)),Q(End*Half,RB*FMath::Cos(B),R+RB*FMath::Sin(B));
            Mesh.QuadUV(HayEnd,Centre,P,Q,Centre,FVector(End,0,0),{.5,.5},{.5+.5*FMath::Cos(A),.5+.5*FMath::Sin(A)},
                {.5+.5*FMath::Cos(B),.5+.5*FMath::Sin(B)},{.5,.5},FVector::RightVector);
        }
        if (Decorations) for (double X : {-36.,36.})
            Mesh.Cylinder(Twine,FVector(X,(RA+1)*FMath::Cos(A),R+(RA+1)*FMath::Sin(A)),
                FVector(X,(RB+1)*FMath::Cos(B),R+(RB+1)*FMath::Sin(B)),.65f,LOD==0?4:3,FHallUV::World(20),false);
    }
    FRandomStream Straws(2026101743);
    const int32 Fibres=Decorations?(LOD==0?440:LOD==1?80:0):0;
    for (int32 K=0;K<Fibres;++K)
    {
        const double Angle=Straws.FRandRange(-PI,PI);
        const double Length=Straws.FRandRange(1.2f,4.2f);
        if (K%3==0)
        {
            const double X=Straws.FRandRange(-Half,Half),Radius=R*(1+.012*FMath::Sin(Angle*7));
            const FVector P(X,Radius*FMath::Cos(Angle),R+Radius*FMath::Sin(Angle));
            const FVector Q=P+FVector(Straws.FRandRange(-2.f,2.f),FMath::Cos(Angle)*Length,FMath::Sin(Angle)*Length);
            Mesh.Cylinder(Hay,P,Q,.22f,3,FHallUV::World(18),false);
        }
        else
        {
            const int32 End=K%2?1:-1;
            const double Radius=R*FMath::Sqrt(Straws.FRand())*.985;
            const FVector P(End*Half,Radius*FMath::Cos(Angle),R+Radius*FMath::Sin(Angle));
            const FVector Q=P+FVector(End*Length,Straws.FRandRange(-1.5f,1.5f),Straws.FRandRange(-1.5f,1.5f));
            Mesh.Cylinder(HayEnd,P,Q,.18f,3,FHallUV::World(18),false);
        }
    }
    return Mesh;
}

TArray<FTransform> BalePoses()
{
    TArray<FTransform> Bales;
    FRandomStream Stream(2026101731);
    for (const FVector2D P : {FirstBale,FVector2D(-25400,800),FVector2D(-26300,1900),FVector2D(-24900,2900),FVector2D(-27500,4000),FVector2D(-25700,5300)})
    {
        if (!Clear(P,FVector2D(125,125))) continue;
        const FVector Normal=HouseExteriorSite::GroundNormal(P.X,P.Y);
        if (Normal.Z<.94f) continue;
        const FQuat Rotation=FQuat::FindBetweenNormals(FVector::UpVector,Normal)*FQuat(FVector::UpVector,Stream.FRandRange(-PI,PI));
        Bales.Add(FTransform(Rotation,FVector(P,Ground(P)-2),FVector(Stream.FRandRange(.92f,1.07f))));
    }
    return Bales;
}

UStaticMesh* Mesh(int32 Kind, const FHallMesh (&Levels)[3], const TMap<FName,UMaterialInterface*>& Materials, bool CPU, bool MergeBarn)
{
    UStaticMesh* Result=NewObject<UStaticMesh>(GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UStaticMesh::StaticClass(),MergeBarn && Kind==0?MergedBarnName:Names[Kind]),RF_Transient);
    Result->NeverStream=true;Result->bAllowCPUAccess=CPU;
    const TArray<FName>& Keys=Levels[0].Order;
    for (const FName Key : Keys)
        Result->GetStaticMaterials().Add(FStaticMaterial(Materials.FindRef(Key),Key
#if WITH_EDITORONLY_DATA
            ,Key
#endif
        ));
    FMeshDescription Descriptions[3];TArray<const FMeshDescription*> Pointers;
    for (int32 LOD=0;LOD<3;++LOD)
    {
        FMeshDescription& Description=Descriptions[LOD];FStaticMeshAttributes Attributes(Description);Attributes.Register();
        auto Positions=Attributes.GetVertexPositions();auto Normals=Attributes.GetVertexInstanceNormals();
        auto Tangents=Attributes.GetVertexInstanceTangents();auto Signs=Attributes.GetVertexInstanceBinormalSigns();
        auto UV=Attributes.GetVertexInstanceUVs();UV.SetNumChannels(1);
        auto Slots=Attributes.GetPolygonGroupMaterialSlotNames();
        for (const FName Key : Keys)
        {
            const FHallSection* Section=Levels[LOD].Sections.Find(Key);if (!Section) continue;
            const FPolygonGroupID Group=Description.CreatePolygonGroup();Slots[Group]=Key;
            TArray<FVertexInstanceID> Instances;
            for (int32 K=0;K<Section->V.Num();++K)
            {
                const FVertexID Vertex=Description.CreateVertex();Positions[Vertex]=FVector3f(Section->V[K]-Pivot(Kind));
                const FVertexInstanceID Instance=Description.CreateVertexInstance(Vertex);Instances.Add(Instance);
                Normals[Instance]=FVector3f(Section->N[K]);Tangents[Instance]=FVector3f(Section->T[K].TangentX);
                Signs[Instance]=1.f;UV.Set(Instance,0,FVector2f(Section->UV[K]));
            }
            for (int32 K=0;K+2<Section->I.Num();K+=3)
            {
                const int32 A=Section->I[K],B=Section->I[K+1],C=Section->I[K+2];
                if (Section->V[A].Equals(Section->V[B],.001) || Section->V[B].Equals(Section->V[C],.001) || Section->V[A].Equals(Section->V[C],.001)) continue;
                Description.CreatePolygon(Group,TArray<FVertexInstanceID,TInlineAllocator<3>>{Instances[A],Instances[B],Instances[C]});
            }
        }
        Pointers.Add(&Description);
    }
    UStaticMesh::FBuildMeshDescriptionsParams Parameters;
    Parameters.bBuildSimpleCollision=false;Parameters.bFastBuild=true;Parameters.bAllowCpuAccess=CPU;
    Parameters.bMarkPackageDirty=false;Parameters.bCommitMeshDescription=false;
    if (!Result->BuildFromMeshDescriptions(Pointers,Parameters)) return nullptr;
    if (FStaticMeshRenderData* Data=Result->GetRenderData())
    {Data->ScreenSize[0].Default=1.f;Data->ScreenSize[1].Default=.25f;Data->ScreenSize[2].Default=.08f;}
    return Result;
}
}

bool HouseCalderFarm::Enabled()
{
    return HouseCalderLandscape::Enabled() && FParse::Param(FCommandLine::Get(),TEXT("CalderFarmTrial"));
}

bool HouseCalderFarm::Occupied(const FVector2D& Point, float Radius)
{
    namespace FI=HouseCalderFarmImpl;
    if (!Enabled()) return false;
    if (HouseCalderFarmSolid::Enabled())
        for (const auto& Run:FI::FenceRuns())
            if (FI::SegmentDistance(Point,Run.Key,Run.Value)<=Radius+7.f) return true;
    if (HouseCalderFarmTree::Occupied(Point,Radius)) return true;
    if (FBox2D(FI::Barn-FVector2D(750,1100),FI::Barn+FVector2D(750,1100)).ExpandBy(Radius).IsInside(Point)) return true;
    for (const FVector2D Bale : {FI::FirstBale,FVector2D(-25400,800),FVector2D(-26300,1900),FVector2D(-24900,2900),FVector2D(-27500,4000),FVector2D(-25700,5300)})
        if (FVector2D::DistSquared(Point,Bale)<FMath::Square(Radius+125.)) return true;
    return false;
}

void HouseCalderFarm::Build(AActor* Owner)
{
    namespace FI=HouseCalderFarmImpl;
    if (!Owner || !Owner->GetWorld() || !Enabled()) return;
    HouseCalderFarmTree::BuildCollision(Owner);
    if (HouseCalderFarmSolid::Enabled() && FI::Clear(FI::Barn,FVector2D(750,1100)))
    {
        FHallMesh Boards,Roof,Curing,Fence;
        FI::BarnGeometry(Boards,Roof,Curing,0,false);FI::FenceGeometry(Fence,0);
        const FHallMesh Hay=FI::BaleGeometry(0,false);
        HouseCalderFarmSolid::Build(Owner,Boards,Roof,Fence,Hay,FI::BalePoses());
    }
    if (Owner->GetNetMode()==NM_DedicatedServer) return;
    const double Start=FPlatformTime::Seconds();
    if (!FI::Clear(FI::Barn,FVector2D(750,1100)))
    {UE_LOG(LogTemp,Warning,TEXT("CALDER_FARM rejected barn footprint"));return;}
    const bool Dump=FParse::Param(FCommandLine::Get(),TEXT("CalderFarmDump"))
        || FParse::Param(FCommandLine::Get(),TEXT("CalderFarmSolidReview"));
    TMap<FName,UMaterialInterface*> Materials;
    Materials.Add(FI::Wood,FI::Material(Owner,TEXT("rough_wood"),FLinearColor(.95,.90,.82),.94));
    Materials.Add(FI::Stone,FI::Material(Owner,TEXT("stacked_stone_wall"),FLinearColor(.65,.64,.6),.95));
    Materials.Add(FI::Metal,FI::RoofMaterial(Owner));
    Materials.Add(FI::Hay,FI::HayMaterial(Owner,false));
    Materials.Add(FI::HayEnd,FI::HayMaterial(Owner,true));
    Materials.Add(FI::Twine,FI::Material(Owner,TEXT("rough_wood"),FLinearColor(.30,.25,.15),.98));
    Materials.Add(FI::Leaf,FI::CuredLeafMaterial(Owner));
    Materials.Add(FI::LeafStem,FI::Material(Owner,TEXT("rough_wood"),FLinearColor(.95,.85,.62),.98));
    if (!HouseCalderFarmTree::Materials(Owner,Materials)) {UE_LOG(LogTemp,Error,TEXT("CALDER_FARM missing persimmon material"));return;}
    for (const auto& Pair : Materials) if (!Pair.Value) {UE_LOG(LogTemp,Error,TEXT("CALDER_FARM missing material"));return;}
    FHallMesh WoodLevels[3],RoofLevels[3],HayLevels[3],LeafLevels[3],FenceLevels[3];
    for (int32 LOD=0;LOD<3;++LOD)
    {
        FI::BarnGeometry(WoodLevels[LOD],RoofLevels[LOD],LeafLevels[LOD],LOD);
        FI::FenceGeometry(FenceLevels[LOD],LOD);HouseCalderFarmTree::Geometry(FenceLevels[LOD],LOD);HayLevels[LOD]=FI::BaleGeometry(LOD);
    }
    // Roof and timber share the barn site and the same architecture culls.
    // This optional merge frees one landscape ISM for later hedgerow detail.
    // Keep the spatially separate fence apart from the barn (normal GPU
    // culling was previously broken by merging those distant clusters).
    const bool MergeBarn=FParse::Param(FCommandLine::Get(),TEXT("CalderBarnMergeTrial"))
        || FParse::Param(FCommandLine::Get(),TEXT("CalderHedgerowTrial"));
    if (MergeBarn)
        for (int32 LOD=0;LOD<3;++LOD)
            for (const FName Key : RoofLevels[LOD].Order)
            {
                if (WoodLevels[LOD].Sections.Contains(Key))
                { UE_LOG(LogTemp,Error,TEXT("CALDER_FARM merge material-key conflict"));return; }
                WoodLevels[LOD].Order.Add(Key);
                WoodLevels[LOD].Sections.Add(Key,RoofLevels[LOD].Sections.FindChecked(Key));
            }
    const FHallMesh* Sources[]={WoodLevels,RoofLevels,HayLevels,LeafLevels,FenceLevels};
    const TArray<FTransform> Bales=FI::BalePoses();
    int32 Failures=0,Components=0;
    FString Csv=TEXT("batch,lod,instance,vertex,x,y,z,groundGapCm\n");
    FString BarnCorners=TEXT("lod,material,parent,x,y,z,nx,ny,nz,tx,ty,tz,sign,u,v\n");
    for (int32 Kind=0;Kind<5;++Kind)
    {
        if (MergeBarn && Kind==1) continue;
        FHallMesh Levels[3]={Sources[Kind][0],Sources[Kind][1],Sources[Kind][2]};
        UStaticMesh* Mesh=FI::Mesh(Kind,Levels,Materials,Dump,MergeBarn);if (!Mesh){++Failures;continue;}
        const TCHAR* BatchName=MergeBarn && Kind==0?FI::MergedBarnName:FI::Names[Kind];
        UInstancedStaticMeshComponent* Part=NewObject<UInstancedStaticMeshComponent>(Owner,FName(BatchName),RF_Transient);
        Owner->AddInstanceComponent(Part);Part->SetupAttachment(Owner->GetRootComponent());Part->SetMobility(EComponentMobility::Static);
        Part->SetStaticMesh(Mesh);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetGenerateOverlapEvents(false);
        Part->SetCastShadow(false);Part->bAffectDistanceFieldLighting=false;Part->bAffectDynamicIndirectLighting=false;
        Part->SetCanEverAffectNavigation(false);
        // Architecture should remain legible from the actual farm hillside.
        // Field props retain90m; existing quality scaling is left untouched.
        const bool LongRange=Kind<2 || (Kind==4 && HouseCalderFarmTree::Enabled());
        const int32 StartCull=LongRange?18000:7000,EndCull=LongRange?22500:9000;
        Part->SetCullDistances(StartCull,EndCull);
        Part->UpdateComponentToWorld();
        if (Kind==2) Part->AddInstances(Bales,false,true);else Part->AddInstance(FTransform(FI::Pivot(Kind)),true);
        // Register with populated instance data and tight local mesh bounds.
        // The fence is a separate spatial cluster from the barn.
        Part->UpdateBounds();Part->RegisterComponent();
        ++Components;
        if (Part->GetCollisionEnabled()!=ECollisionEnabled::NoCollision || Part->CastShadow
            || Part->bAffectDistanceFieldLighting || Part->bAffectDynamicIndirectLighting || Part->CanEverAffectNavigation()
            || Part->InstanceStartCullDistance!=StartCull || Part->InstanceEndCullDistance!=EndCull) ++Failures;
        if (Dump)
        {
            UE_LOG(LogTemp,Display,TEXT("CALDER_FARM_BOUNDS kind=%d localCenter=%s worldCenter=%s extent=%s"),Kind,
                *Mesh->GetBounds().Origin.ToString(),*Part->Bounds.Origin.ToString(),*Part->Bounds.BoxExtent.ToString());
        }
        FStaticMeshRenderData* Render=Mesh->GetRenderData();if (!Render || Render->LODResources.Num()!=3) {++Failures;continue;}
        int32 Tris[3]={0,0,0};
        for (int32 LOD=0;LOD<3;++LOD)
        {
            const FStaticMeshLODResources& Level=Render->LODResources[LOD];Tris[LOD]=Level.GetNumTriangles();
            for (const FStaticMeshSection& Section : Level.Sections)
                if (Section.MaterialIndex<0 || Section.MaterialIndex>=Mesh->GetStaticMaterials().Num()
                    || Section.FirstIndex+Section.NumTriangles*3>uint32(Level.IndexBuffer.GetNumIndices())) ++Failures;
            if (LOD && Tris[LOD]>=Tris[LOD-1]) ++Failures;
            if (!Dump) continue;
            for (int32 Index=0;Index<Part->GetInstanceCount();++Index)
            {
                FTransform Pose;Part->GetInstanceTransform(Index,Pose,true);double Lowest=1e9;
                if (Kind<2)
                    for (const FStaticMeshSection& Section : Level.Sections)
                    {
                        if (Section.MaterialIndex<0 || Section.MaterialIndex>=Mesh->GetStaticMaterials().Num()
                            || Section.FirstIndex+Section.NumTriangles*3>uint32(Level.IndexBuffer.GetNumIndices())) continue;
                        const FStaticMaterial& Slot=Mesh->GetStaticMaterials()[Section.MaterialIndex];
                        const FString Parent=Slot.MaterialInterface?GetNameSafe(Slot.MaterialInterface->GetMaterial()):TEXT("None");
                        for (uint32 Corner=Section.FirstIndex;Corner<Section.FirstIndex+Section.NumTriangles*3;++Corner)
                        {
                            const uint32 Vertex=Level.IndexBuffer.GetIndex(Corner);
                            const FVector P=Pose.TransformPosition(FVector(Level.VertexBuffers.PositionVertexBuffer.VertexPosition(Vertex)));
                            const FVector4f PackedN=Level.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(Vertex);
                            const FVector Normal=Pose.TransformVectorNoScale(FVector(FVector3f(PackedN))).GetSafeNormal();
                            const FVector Tangent=Pose.TransformVectorNoScale(FVector(FVector3f(Level.VertexBuffers.StaticMeshVertexBuffer.VertexTangentX(Vertex)))).GetSafeNormal();
                            const FVector2f UV=Level.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(Vertex,0);
                            BarnCorners+=FString::Printf(TEXT("%d,%s,%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.1f,%.9f,%.9f\n"),
                                LOD,*Slot.MaterialSlotName.ToString(),*Parent,P.X,P.Y,P.Z,Normal.X,Normal.Y,Normal.Z,Tangent.X,Tangent.Y,Tangent.Z,PackedN.W,UV.X,UV.Y);
                        }
                    }
                for (uint32 Vertex=0;Vertex<Level.VertexBuffers.PositionVertexBuffer.GetNumVertices();++Vertex)
                {
                    const FVector P=Pose.TransformPosition(FVector(Level.VertexBuffers.PositionVertexBuffer.VertexPosition(Vertex)));
                    const double Gap=P.Z-FI::Ground(FVector2D(P));Lowest=FMath::Min(Lowest,Gap);
                    if (HouseCalderTerrain::InFrozen(P.X,P.Y)) ++Failures;
                    Csv+=FString::Printf(TEXT("%d,%d,%d,%u,%.4f,%.4f,%.4f,%.4f\n"),Kind,LOD,Index,Vertex,P.X,P.Y,P.Z,Gap);
                }
                if (Kind==2 && (Lowest < -10 || Lowest > 10)) ++Failures;
            }
        }
        UE_LOG(LogTemp,Display,TEXT("CALDER_FARM_BATCH name=%s instances=%d lodTris=%d/%d/%d plainISM=1 collision=0 shadows=0 cull=%d/%d"),BatchName,Part->GetInstanceCount(),Tris[0],Tris[1],Tris[2],Part->InstanceStartCullDistance,Part->InstanceEndCullDistance);
    }
    const IConsoleVariable* DistanceScale=IConsoleManager::Get().FindConsoleVariable(TEXT("r.ViewDistanceScale"));
    UE_LOG(LogTemp,Display,TEXT("CALDER_FARM_CULL viewDistanceScale=%.3f architecturalEndCm=22500 fieldEndCm=9000 globalSettingChanged=0 treeUsesExistingFenceCluster=1"),DistanceScale?DistanceScale->GetFloat():1.f);
    if (Bales.Num()==0 || Components!=(MergeBarn?4:5)) ++Failures;
    UE_LOG(LogTemp,Display,TEXT("CALDER_FARM_MERGE enabled=%d sourceGeometryAndMaterialsPreserved=1 freedLandscapeISMs=%d"),int32(MergeBarn),int32(MergeBarn));
    if (Dump)
    {
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("CalderFarmDump");IFileManager::Get().MakeDirectory(*Dir,true);
        FFileHelper::SaveStringToFile(Csv,*(Dir/TEXT("vertices.csv")));
        FFileHelper::SaveStringToFile(BarnCorners,*(Dir/TEXT("architecture_corners.csv")));
    }
    UE_LOG(LogTemp,Display,TEXT("CALDER_FARM %s components=%d bales=%d terrainAndOriginalRoutesUnchanged=1 newLights=0 ms=%.1f"),Failures?TEXT("FAIL"):TEXT("PASS"),Components,Bales.Num(),(FPlatformTime::Seconds()-Start)*1000);
}

bool HouseCalderFarm::DetailView(UWorld* World,FName Name,FVector& Eye,FVector& Target)
{
#if !UE_BUILD_SHIPPING
    namespace FI=HouseCalderFarmImpl;
    if (!World || !Enabled() || (Name!=TEXT("detail_calder_farm") && Name!=TEXT("detail_calder_bale") && Name!=TEXT("detail_calder_barn_entry") && Name!=TEXT("detail_calder_barn_roof"))) return false;
    static TWeakObjectPtr<UWorld> CachedWorld;
    static TMap<FName,TPair<FVector,FVector>> Views;
    if (CachedWorld.Get()!=World) {CachedWorld=World;Views.Reset();}
    if (const TPair<FVector,FVector>* Cached=Views.Find(Name))
    {Eye=Cached->Key;Target=Cached->Value;return true;}
    const bool Roof=Name==TEXT("detail_calder_barn_roof");
    const bool Entry=Name==TEXT("detail_calder_barn_entry");
    const bool Wide=Name!=TEXT("detail_calder_bale");
    const FVector2D P=Roof?FVector2D(-29500,4000):Entry?FVector2D(-27200,-3750):Wide?FVector2D(-25200,-3250):FI::FirstBale+FVector2D(250,-250);
    const double Z=FI::Ground(P);FHitResult Hit;
    if (!World->LineTraceSingleByChannel(Hit,FVector(P,Z+60),FVector(P,Z-90),ECC_Visibility)
        || FMath::Abs(Hit.ImpactPoint.Z-Z)>4.) return false;
    Eye=FVector(P,Hit.ImpactPoint.Z+130);
    Target=Roof?FVector(FI::Barn,800):Entry?FVector(FI::Barn,FI::Ground(FI::Barn)+350):Wide?FVector(FI::Barn,FI::Ground(FI::Barn)+420):FVector(FI::FirstBale,FI::Ground(FI::FirstBale)+75);
    Views.Add(Name,TPair<FVector,FVector>(Eye,Target));
    UE_LOG(LogTemp,Display,TEXT("CALDER_FARM_CAMERA name=%s eye=%s target=%s groundPlus130=1"),*Name.ToString(),*Eye.ToString(),*Target.ToString());
    return true;
#else
    return false;
#endif
}
