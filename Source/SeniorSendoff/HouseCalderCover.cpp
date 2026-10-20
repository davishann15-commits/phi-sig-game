#include "HouseCalderCover.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderFarm.h"
#include "HouseCalderLandform.h"
#include "HouseCalderLandscape.h"
#include "HouseCalderRoadside.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderWater.h"
#include "HouseCampusLayout.h"
#include "HouseCampusStaticMesh.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseExteriorCampus.h"
#include "HouseLeafWind.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Crc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "PhysicsEngine/BodyInstance.h"
#include "ProceduralMeshComponent.h"

namespace HouseCalderCoverImpl
{
constexpr float BucketSize = 400.f, Radius = 110.f;
constexpr int32 MaxInstances = 120000;
struct FDump { bool bEnabled=false; FString Rejected; };
FDump Dump;
const FName Keys[] = { TEXT("CC_Lawn"), TEXT("CC_Tawny"), TEXT("CC_Stem"), TEXT("CC_Aster"), TEXT("CC_Seed") };
const TCHAR* const Names[] = { TEXT("CalderCover_Lawn"), TEXT("CalderCover_Broomsedge"), TEXT("CalderCover_Aster"), TEXT("CalderCover_Fluff") };

struct FRejectGrid
{
    TArray<FBox2D> Boxes;
    TMap<FIntPoint, TArray<int32>> Buckets;
    void Add(FBox2D Box)
    {
        Box = Box.ExpandBy(Radius);
        const int32 Index = Boxes.Add(Box);
        for (int32 Y = FMath::FloorToInt(Box.Min.Y / BucketSize); Y <= FMath::FloorToInt(Box.Max.Y / BucketSize); ++Y)
            for (int32 X = FMath::FloorToInt(Box.Min.X / BucketSize); X <= FMath::FloorToInt(Box.Max.X / BucketSize); ++X)
                Buckets.FindOrAdd(FIntPoint(X, Y)).Add(Index);
    }
    bool Taken(const FVector2D& P) const
    {
        const TArray<int32>* Found = Buckets.Find(FIntPoint(FMath::FloorToInt(P.X / BucketSize), FMath::FloorToInt(P.Y / BucketSize)));
        if (Found) for (const int32 Index : *Found) if (Boxes[Index].IsInside(P)) return true;
        return false;
    }
};

double SegmentDistance(const FVector2D& P, const FVector2D& A, const FVector2D& B)
{
    const FVector2D AB = B - A;
    const double T = AB.SizeSquared() > .001 ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / AB.SizeSquared(), 0., 1.) : 0.;
    return FVector2D::Distance(P, A + AB * T);
}

bool PathNear(const FVector2D& P)
{
    for (const HouseCalderLandform::FLandformPath& Path : HouseCalderLandform::LandformPaths())
    {
        const int32 Segments = Path.bClosed ? Path.Points.Num() : Path.Points.Num() - 1;
        for (int32 I = 0; I < Segments; ++I)
            if (SegmentDistance(P, Path.Points[I], Path.Points[(I + 1) % Path.Points.Num()]) <= Path.Width * .5 + Radius) return true;
    }
    for (const HouseCalderLandform::FRoadsideRun& Ditch : HouseCalderLandform::Ditches())
        for (int32 I = 1; I < Ditch.Points.Num(); ++I)
            if (SegmentDistance(P, Ditch.Points[I - 1], Ditch.Points[I]) <= Radius + 70.f) return true;
    return false;
}

// Alpha-preserving, coverage-scaled mip chain, matching the existing foliage painter.
UTexture2D* SeedMask(UHouseBlackHallSubsystem* L, FName K, const FHallImage& I)
{
    UTexture2D* T = UTexture2D::CreateTransient(I.W,I.H,PF_B8G8R8A8,MakeUniqueObjectName(GetTransientPackage(),UTexture2D::StaticClass(),K));
    if (!T) return nullptr;
    T->SRGB = true; T->CompressionSettings = TC_Default;
    T->AddressX = T->AddressY = TA_Clamp; T->Filter = TF_Trilinear; T->LODGroup = TEXTUREGROUP_World;
    FTexturePlatformData* PD = T->GetPlatformData();
    if (!PD || PD->Mips.Num() == 0) return nullptr;
    TArray<FLinearColor> Level = I.Px;
    int W = I.W, H = I.H;
    float Coverage = 0.f;
    for (const FLinearColor& C : Level) Coverage += C.A >= .32f ? 1.f : 0.f;
    Coverage /= Level.Num();
    for (int Mip=0;;++Mip)
    {
        float Scale = 1.f;
        if (Mip > 0 && W*H > 16)
        {
            float Lo = .05f, Hi = 4.f;
            for (int Search=0;Search<8;++Search)
            {
                const float Mid = (Lo+Hi)*.5f;
                int Hits=0; for (const FLinearColor& C : Level) Hits += C.A*Mid >= .32f ? 1 : 0;
                if (Hits / float(Level.Num()) < Coverage) Lo=Mid; else Hi=Mid;
            }
            Scale=(Lo+Hi)*.5f;
        }
        TArray<FColor> Pixels; Pixels.SetNumUninitialized(W*H);
        for (int J=0;J<Level.Num();++J)
        {
            FLinearColor C = Level[J]; C.A = FMath::Clamp(C.A*Scale,0.f,1.f); Pixels[J]=C.ToFColor(true);
        }
        FTexture2DMipMap* Target = Mip==0 ? &PD->Mips[0] : new FTexture2DMipMap(W,H,1);
        if (Mip>0) PD->Mips.Add(Target);
        Target->SizeX=W; Target->SizeY=H;
        Target->BulkData.Lock(LOCK_READ_WRITE);
        void* Data=Target->BulkData.Realloc(int64(W)*H*4);
        FMemory::Memcpy(Data,Pixels.GetData(),int64(W)*H*4); Target->BulkData.Unlock();
        if (W==1 && H==1) break;
        const int NW=FMath::Max(1,W/2), NH=FMath::Max(1,H/2);
        TArray<FLinearColor> Next; Next.SetNumUninitialized(NW*NH);
        for(int Y=0;Y<NH;++Y)for(int X=0;X<NW;++X)
        {
            FLinearColor C(0,0,0,0); float Alpha=0;
            for(int V=0;V<2;++V)for(int U=0;U<2;++U)
            { const FLinearColor P=Level[FMath::Min(H-1,Y*2+V)*W+FMath::Min(W-1,X*2+U)]; C.R+=P.R*P.A;C.G+=P.G*P.A;C.B+=P.B*P.A;Alpha+=P.A; }
            if(Alpha>1e-5f){C.R/=Alpha;C.G/=Alpha;C.B/=Alpha;} C.A=Alpha*.25f;Next[Y*NW+X]=C;
        }
        Level=MoveTemp(Next);W=NW;H=NH;
    }
    T->UpdateResource();L->Textures.Add(K,T);return T;
}

bool LookReady(UHouseBlackHallSubsystem* Look)
{
    UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NatureGrassBlades.M_NatureGrassBlades"));
    if (!Parent) return false;
    const FLinearColor Tints[] = { FLinearColor(.105f,.18f,.035f), FLinearColor(.30f,.225f,.09f), FLinearColor(.14f,.19f,.045f),
        FLinearColor(.29f,.13f,.34f), FLinearColor(.49f,.43f,.28f) };
    for (int32 I = 0; I < UE_ARRAY_COUNT(Keys); ++I)
    {
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, Look);
        Material->SetVectorParameterValue(TEXT("Tint"), Tints[I]);
        Material->SetScalarParameterValue(TEXT("WindStrength"), I == 0 ? 1.3f : 2.4f);
        Look->Materials.Add(Keys[I], Material);
    }
    // A code-painted, masked seed spray. Real irregular awns and gaps replace
    // the opaque diamond silhouette; no acquired or shared asset is edited.
    UMaterialInterface* Foliage=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NatureFoliage.M_NatureFoliage"));
    if (!Foliage) return false;
    FHallImage Spray; Spray.Init(128,384,FLinearColor(.48f,.37f,.20f,0.f));
    FRandomStream Paint(2026101751);
    Spray.Stroke({FVector2D(64,378),FVector2D(62,210),FVector2D(64,18)},1.5f,FLinearColor(.24f,.16f,.065f,1.f));
    for (int32 I=0;I<180;++I)
    {
        const float Y=Paint.FRandRange(30.f,354.f),T=(Y-30.f)/324.f;
        const float Envelope=FMath::Sin(T*PI)*Paint.FRandRange(15.f,49.f);
        const float Side=I%2?1.f:-1.f;
        const FVector2D A(64.f+Paint.FRandRange(-3.f,3.f),Y),B(64.f+Side*Envelope,Y-Paint.FRandRange(12.f,42.f));
        const float Tone=Paint.FRandRange(.65f,1.1f);
        const FLinearColor Colour(.54f*Tone,.43f*Tone,.26f*Tone,1.f);
        Spray.Stroke({A,(A+B)*.5+FVector2D(Side*3.f,-3.f),B},Paint.FRandRange(.55f,1.1f),Colour);
        if (I%3==0) Spray.Ellipse(A+FVector2D(Side*4.f,-2.f),FVector2D(1.5f,3.f),Side*25.f,Colour);
    }
    UTexture2D* Albedo=SeedMask(Look,TEXT("T_CC_SeedSpray"),Spray);
    TArray<float> Height; Height.Reserve(Spray.Px.Num());
    for (const FLinearColor& Pixel : Spray.Px) Height.Add(Pixel.A*.3f);
    FHallImage Normals; HallHeightToNormal(Height,Spray.W,Spray.H,.6f,false,Normals);
    UTexture2D* Normal=HallMakeTexture(Normals,false,true,TEXT("T_CC_SeedSpray_N"),false);
    if (!Albedo || !Normal) return false;
    Look->Textures.Add(TEXT("T_CC_SeedSpray_N"),Normal);
    UMaterialInstanceDynamic* Seeds=UMaterialInstanceDynamic::Create(Foliage,Look);
    Seeds->SetTextureParameterValue(TEXT("BaseMap"),Albedo);Seeds->SetTextureParameterValue(TEXT("NormalMap"),Normal);
    Seeds->SetScalarParameterValue(TEXT("WindStrength"),2.4f);Seeds->SetScalarParameterValue(TEXT("Transmission"),.28f);
    Look->Materials.Add(Keys[4],Seeds);
    return true;
}

void Triangle(FHallMesh& Mesh, FName Key, const FVector& A, const FVector& B, const FVector& C, const FVector& N)
{
    FHallSection& S = Mesh.Sec(Key);
    const int32 First = S.V.Num();
    S.V.Append({A,B,C}); S.N.Append({N,N,N});
    S.UV.Append({FVector2D(0,.45),FVector2D(1,.45),FVector2D(.5,0)});
    const FVector Tangent = (B - A).GetSafeNormal();
    for (int32 I = 0; I < 3; ++I) S.T.Add(FProcMeshTangent(Tangent, false));
    if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), N) < 0.) S.I.Append({First,First+1,First+2});
    else S.I.Append({First,First+2,First+1});
}

FVector Blade(FHallMesh& Mesh, FName Key, FVector Base, float Angle, float Height, float Width, float Lean)
{
    const FVector U(FMath::Cos(Angle)*Width,FMath::Sin(Angle)*Width,0);
    const FVector Bend(-FMath::Sin(Angle)*Lean,FMath::Cos(Angle)*Lean,0);
    const FVector Mid = Base + Bend*.3f + FVector(0,0,Height*.55f), Tip = Base + Bend + FVector(0,0,Height);
    const FVector N = (FVector(-FMath::Sin(Angle),FMath::Cos(Angle),0)*.45f+FVector::UpVector).GetSafeNormal();
    Mesh.QuadUV(Key,Base-U,Base+U,Mid+U*.5f,Mid-U*.5f,N,FVector2D(0,1),FVector2D(1,1),FVector2D(1,.45),FVector2D(0,.45),U.GetSafeNormal());
    Triangle(Mesh,Key,Mid-U*.5f,Mid+U*.5f,Tip,N);
    return Tip;
}

void Head(FHallMesh& Mesh, FName Key, FVector Centre, float Size, int32 Petals)
{
    for (int32 I = 0; I < Petals; ++I)
    {
        const float Angle = I * 2.f * PI / Petals;
        const FVector U(FMath::Cos(Angle),FMath::Sin(Angle),.12), V(-FMath::Sin(Angle),FMath::Cos(Angle),0);
        Mesh.QuadUV(Key,Centre,Centre+U*Size-V*Size*.2f+FVector(0,0,Size*.15f),Centre+U*(Size*1.45f),Centre+U*Size+V*Size*.2f+FVector(0,0,Size*.15f),
            FVector::UpVector,FVector2D(.5,0),FVector2D(0,0),FVector2D(.5,0),FVector2D(1,0),U.GetSafeNormal());
    }
}

void Panicle(FHallMesh& Mesh,FVector Tip,float Width,float Height)
{
    for (int32 I=0;I<2;++I)
    {
        const FVector U=I?FVector(0,Width,0):FVector(Width,0,0);
        const FVector Top=Tip+FVector(0,0,Height);
        const FVector N=I?FVector(1,0,.3):FVector(0,1,.3);
        Mesh.QuadUV(Keys[4],Tip-U,Tip+U,Top+U,Top-U,N.GetSafeNormal(),FVector2D(0,1),FVector2D(1,1),FVector2D(1,0),FVector2D(0,0),U.GetSafeNormal());
    }
}

FHallMesh Tuft(int32 Kind)
{
    FHallMesh Mesh; FRandomStream R(2026101710 + Kind);
    if (Kind < 2)
    {
        for (int32 I = 0; I < (Kind?16:18); ++I)
        {
            const bool bSeed=Kind && I>=14;
            const FVector Tip=Blade(Mesh,Keys[Kind],FVector(R.FRandRange(-7,7),R.FRandRange(-7,7),0),R.FRandRange(0,2*PI),
                R.FRandRange(bSeed?62.f:Kind?26.f:9.f,bSeed?78.f:Kind?58.f:19.f),
                R.FRandRange(bSeed?.25f:.55f,bSeed?.45f:1.15f),R.FRandRange(bSeed?3.f:Kind?14.f:7.f,bSeed?7.f:Kind?35.f:19.f));
            if (bSeed) Panicle(Mesh,Tip,2.5f,10.f);
        }
    }
    else if (Kind == 2)
    {
        for (int32 I = 0; I < 2; ++I)
        {
            const FVector Base(I*18-9,I*9-4,0);
            const FVector Tip=Blade(Mesh,Keys[2],Base,.6f+I,47.f+I*9,.4f,2.f);
            Head(Mesh,Keys[3],Tip,1.3f,8);
            // A solid, small ochre disc uses the existing tawny blade material;
            // the seed-spray mask is only for upright field panicles.
            Head(Mesh,Keys[1],Tip+FVector(0,0,.2),.35f,2);
            for (int32 Leaf=0;Leaf<2;++Leaf) Blade(Mesh,Keys[2],Base+FVector(0,0,12+Leaf*10),I+Leaf*PI,14.f,1.3f,13.f);
        }
    }
    else
    {
        for (int32 I = 0; I < 4; ++I)
        {
            const FVector Base(I*10-15,(I%2)*12-6,0);
            const FVector Tip=Blade(Mesh,Keys[2],Base,I,55.f+I*4,.4f,3.f);
            Panicle(Mesh,Tip,4.f,12.f);
            Blade(Mesh,Keys[2],Base+FVector(0,0,15),I+1.f,22.f,1.2f,16.f);
        }
    }
    return Mesh;
}

bool Clear(AActor* Owner, const FRejectGrid& Grid, const FVector2D& P, float& Z, FVector& Normal)
{
    // The stadium and its5m apron are outside the layout's normal block/pave
    // tables. Match the legacy lawn exclusion, expanded for the whole tuft.
    // Its apron and ground can be coplanar, so physics hit ordering alone is
    // not a deterministic paving classifier here.
    const FVector2D StadiumDelta(P.X-HouseCampus::StadiumX,P.Y-HouseCampus::StadiumY);
    if (FMath::Square(StadiumDelta.X/(6100.f+Radius))+FMath::Square(StadiumDelta.Y/(4300.f+Radius))<1.) return false;
    if (HouseCalderFarm::Occupied(P,Radius) || Grid.Taken(P) || HouseCampusLayout::PavingNear(P,Radius) || HouseCalderFeatures::InStair(P,Radius)
        || HouseCalderFeatures::InRamp(P,Radius) || PathNear(P) || HouseCalderRoadside::Occupied(P,Radius)) return false;
    // Keep the full wind-expanded footprint back from the shore and decks,
    // including diagonal corners, instead of testing only the root point.
    for (const double DX : {-double(Radius),0.,double(Radius)})
        for (const double DY : {-double(Radius),0.,double(Radius)})
        {
            float WaterZ=0.f;
            const FVector2D Probe=P+FVector2D(DX,DY);
            if (HouseCalderWater::WaterAt(Probe,WaterZ) || HouseCalderWater::DeckAt(Probe,WaterZ)) return false;
        }
    Z = HouseExteriorSite::GroundHeight(P.X,P.Y); Normal = HouseExteriorSite::GroundNormal(P.X,P.Y);
    if (Normal.Z < .82) return false;
    for (const FVector2D& Offset : {FVector2D(Radius,0),FVector2D(-Radius,0),FVector2D(0,Radius),FVector2D(0,-Radius)})
    {
        const float Expected = Z - float((Normal.X*Offset.X+Normal.Y*Offset.Y)/Normal.Z);
        if (FMath::Abs(HouseExteriorSite::GroundHeight(P.X+Offset.X,P.Y+Offset.Y)-Expected)>8.f) return false;
    }
    // Static scenery defines the meadow. Pawns and movable replicated props
    // may be at different positions while peers finish loading the level.
    FHitResult Hit;
    const bool bHit=Owner->GetWorld()->LineTraceSingleByObjectType(Hit,FVector(P,Z+120),FVector(P,Z-100),FCollisionObjectQueryParams(ECC_WorldStatic),
        FCollisionQueryParams(SCENE_QUERY_STAT(CalderCoverPlacement),true));
    const bool bPass=bHit && Hit.GetComponent() && Hit.GetComponent()->GetName().StartsWith(TEXT("CalderTerrainCollision_")) && FMath::Abs(Hit.ImpactPoint.Z-Z)<=3.f;
    if (!bPass && Dump.bEnabled)
    {
        const UPrimitiveComponent* Component=Hit.GetComponent();
        Dump.Rejected+=FString::Printf(TEXT("%.6f,%.6f,%.6f,%d,%.6f,%s,%s,%s,%d,%d\n"),P.X,P.Y,Z,int32(bHit),Hit.ImpactPoint.Z,
            Component?*Component->GetName():TEXT("none"),Hit.GetActor()?*Hit.GetActor()->GetName():TEXT("none"),
            Hit.GetActor()?*Hit.GetActor()->GetClass()->GetName():TEXT("none"),Component?int32(Component->Mobility):-1,Component?int32(Component->GetCollisionObjectType()):-1);
    }
    return bPass;
}
}

int32 HouseCalderCover::Build(AActor* Owner, UHouseBlackHallSubsystem* Look)
{
    if (!Owner || !Owner->GetWorld() || !Look || !HouseCalderLandscape::Enabled() || Owner->GetNetMode()==NM_DedicatedServer) return 0;
    const double Start=FPlatformTime::Seconds();
    HouseCalderCoverImpl::Dump.bEnabled=FParse::Param(FCommandLine::Get(),TEXT("CalderCoverDump"));
    HouseCalderCoverImpl::Dump.Rejected=TEXT("x,y,z,hit,hitZ,component,actor,class,mobility,objectType\n");
    if (!HouseCalderCoverImpl::LookReady(Look)) { UE_LOG(LogTemp,Error,TEXT("CALDER_COVER missing grass master")); return 0; }
    HouseCalderCoverImpl::FRejectGrid Grid;
    for (const FBox2D& Box : HouseCalderTerrain::FrozenRects()) Grid.Add(Box);
    for (const HouseCampusLayout::FBlock& Block : HouseCampusLayout::Blocks()) Grid.Add(Block.Rect());
    for (const HouseCalderLandform::FReserve& Reserve : HouseCalderLandform::Reserves()) Grid.Add(FBox2D(Reserve.Centre-Reserve.Size*.5,Reserve.Centre+Reserve.Size*.5));
    TArray<FTransform> Placements[4]; FRandomStream Random(2026101701);
    int32 Count=0;
    // Four candidates per4m bucket: <=102,400 before rejection. The extra
    // independent stream preserves all three original candidate positions.
    for (int32 Y=-80;Y<80;++Y) for (int32 X=-80;X<80;++X) for (int32 Slot=0;Slot<4;++Slot)
    {
        FRandomStream Extra(2026101742+(Y+80)*160+X+80);
        FRandomStream& Stream=Slot<3?Random:Extra;
        const FVector2D P((X+Stream.FRandRange(.15f,.85f))*400.,(Y+Stream.FRandRange(.15f,.85f))*400.);
        const float Chance=Stream.FRand(),Yaw=Stream.FRandRange(-PI,PI),Scale=Stream.FRandRange(.8f,1.2f);
        float Z=0.f; FVector N;
        if (Chance>.88f || !HouseCalderCoverImpl::Clear(Owner,Grid,P,Z,N)) continue;
        const bool bRough=N.Z<.948683f || P.Y>19800 || P.X>20000 || P.X<-21000; // steeper than1:3 stays long
        const int32 Kind=bRough ? (Chance<.025f?2:Chance<.05f?3:1) : 0;
        const FQuat Rotation=FQuat::FindBetweenNormals(FVector::UpVector,N)*FQuat(FVector::UpVector,Yaw);
        Placements[Kind].Add(FTransform(Rotation,FVector(P,Z+.15f),FVector(Scale)));
        if (++Count==HouseCalderCoverImpl::MaxInstances) break;
    }
    int32 Built=0;
    uint32 PlacementHash=0;
    for (int32 Kind=0;Kind<4;++Kind)
    {
        FHallMesh Geometry=HouseCalderCoverImpl::Tuft(Kind);
        if (Geometry.NumTriangles()>60) { UE_LOG(LogTemp,Error,TEXT("CALDER_COVER tuft exceeds60triangles")); return 0; }
        UStaticMesh* Mesh=HouseCampusStaticMesh::Build(Geometry,FName(HouseCalderCoverImpl::Names[Kind]),Look);
        if (!Mesh) continue;
        UInstancedStaticMeshComponent* Component=NewObject<UInstancedStaticMeshComponent>(Owner,FName(HouseCalderCoverImpl::Names[Kind]),RF_Transient);
        Owner->AddInstanceComponent(Component); Component->SetupAttachment(Owner->GetRootComponent());
        Component->SetMobility(EComponentMobility::Static);Component->SetStaticMesh(Mesh);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);Component->SetGenerateOverlapEvents(false);
        Component->SetCastShadow(false);Component->bAffectDistanceFieldLighting=false;Component->bAffectDynamicIndirectLighting=false;
        Component->SetCanEverAffectNavigation(false);Component->SetCullDistances(2500,Kind==0?3500:4500);
        Component->RegisterComponent();Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->AddInstances(Placements[Kind],false,true);
        for (const FTransform& Transform : Placements[Kind])
        {
            const FVector Position=Transform.GetLocation(),Scale=Transform.GetScale3D();
            const FQuat Rotation=Transform.GetRotation();
            const int32 Values[]={Kind,FMath::RoundToInt(Position.X*100.),FMath::RoundToInt(Position.Y*100.),FMath::RoundToInt(Position.Z*100.),
                FMath::RoundToInt(Scale.X*10000.),FMath::RoundToInt(Rotation.X*100000.),FMath::RoundToInt(Rotation.Y*100000.),
                FMath::RoundToInt(Rotation.Z*100000.),FMath::RoundToInt(Rotation.W*100000.)};
            PlacementHash=FCrc::MemCrc32(Values,sizeof(Values),PlacementHash);
        }
        Built+=Placements[Kind].Num();
        UE_LOG(LogTemp,Display,TEXT("CALDER_COVER_BATCH name=%s instances=%d triangles=%d plainISM=1 collision=0 shadows=0"),
            HouseCalderCoverImpl::Names[Kind],Placements[Kind].Num(),Geometry.NumTriangles());
    }
    if (HouseCalderCoverImpl::Dump.bEnabled)
    {
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("CalderCoverDump");
        IFileManager::Get().MakeDirectory(*Dir,true);
        FString Csv=TEXT("kind,instance,x,y,z,qx,qy,qz,qw,scale\n");
        for (int32 Kind=0;Kind<4;++Kind) for (int32 I=0;I<Placements[Kind].Num();++I)
        {
            const FTransform& T=Placements[Kind][I];const FVector P=T.GetLocation();const FQuat Q=T.GetRotation();
            Csv+=FString::Printf(TEXT("%d,%d,%.6f,%.6f,%.6f,%.9f,%.9f,%.9f,%.9f,%.6f\n"),Kind,I,P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,T.GetScale3D().X);
        }
        FFileHelper::SaveStringToFile(Csv,*(Dir/TEXT("placements.csv")));
        FFileHelper::SaveStringToFile(HouseCalderCoverImpl::Dump.Rejected,*(Dir/TEXT("rejected_traces.csv")));
        UE_LOG(LogTemp,Display,TEXT("CALDER_COVER_DUMP instances=%d begunPlay=%d ownerBegunPlay=%d worldTime=%.3f netMode=%d"),Built,
            int32(Owner->GetWorld()->HasBegunPlay()),int32(Owner->HasActorBegunPlay()),Owner->GetWorld()->GetTimeSeconds(),int32(Owner->GetNetMode()));
    }
    HouseCalderCoverImpl::Dump.Rejected.Empty();
    Owner->Tags.AddUnique(TEXT("CalderCoverBuilt"));
    UE_LOG(LogTemp,Display,TEXT("CALDER_COVER instances=%d batches=4 maxTuftTriangles=60 rejectBucketCm=400 seed=2026101701 ms=%.1f frozenUntouched=1 placementHash=%08x"),Built,(FPlatformTime::Seconds()-Start)*1000.,PlacementHash);
    return Built;
}

void HouseCalderCover::RegisterWind(AHouseLeafWind* Wind,UHouseBlackHallSubsystem* Look)
{
    if (!Wind || !Look || !HouseCalderLandscape::Enabled()) return;
    for (const FName Key : HouseCalderCoverImpl::Keys)
        if (UMaterialInstanceDynamic* Material=Cast<UMaterialInstanceDynamic>(Look->Mat(Key))) Wind->RegisterWindMaterial(Material,Key==HouseCalderCoverImpl::Keys[0]?1.3f:2.4f);
}

void HouseCalderCover::ScheduleBuild(AActor* Owner,UHouseBlackHallSubsystem* Look)
{
    if (!Owner || !Look || !Owner->GetWorld() || !HouseCalderLandscape::Enabled() || Owner->GetNetMode()==NM_DedicatedServer
        || Owner->ActorHasTag(TEXT("CalderCoverQueued"))) return;
    Owner->Tags.AddUnique(TEXT("CalderCoverQueued"));
    const TWeakObjectPtr<AActor> WeakOwner(Owner);
    const TWeakObjectPtr<UHouseBlackHallSubsystem> WeakLook(Look);
    const double Started=FPlatformTime::Seconds();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakOwner,WeakLook,Started](float)->bool
    {
        if (!WeakOwner.IsValid() || !WeakLook.IsValid() || !WeakOwner->GetWorld()) return false;
        UWorld* World=WeakOwner->GetWorld();
        bool bReady=World->HasBegunPlay() && WeakOwner->HasActorBegunPlay() && HouseCalderTerrain::CollisionReady(World);
        for (TActorIterator<AActor> It(World);It;++It)
        {
            TInlineComponentArray<UProceduralMeshComponent*> Parts(*It);
            for (UProceduralMeshComponent* Part : Parts)
            {
                if (!Part->GetName().StartsWith(TEXT("CampusPolish")) || Part->GetCollisionEnabled()==ECollisionEnabled::NoCollision) continue;
                const FBodyInstance* Body=Part->GetBodyInstance();
                if (!Body || !Body->IsValidBodyInstance()) bReady=false;
            }
        }
        if (!bReady)
        {
            if (FPlatformTime::Seconds()-Started<60.) return true;
            UE_LOG(LogTemp,Error,TEXT("CALDER_COVER collision readiness timed out; no scatter built"));
            return false;
        }
        UE_LOG(LogTemp,Display,TEXT("CALDER_COVER_COLLISION_READY waitMs=%.1f"),(FPlatformTime::Seconds()-Started)*1000.);
        HouseCalderCover::Build(WeakOwner.Get(),WeakLook.Get());
        for (TActorIterator<AHouseLeafWind> Wind(World);Wind;++Wind)
            if (Wind->GetOwner()==WeakOwner.Get()) HouseCalderCover::RegisterWind(*Wind,WeakLook.Get());
        return false;
    }),.1f);
}
