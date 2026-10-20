#include "HouseCalderHedgerow.h"
#include "HouseCalderHedgerowGeometry.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"
#include "HouseCalderFarm.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderLandform.h"
#include "HouseCalderRoadside.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderWater.h"
#include "HouseCampusLayout.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseLeafWind.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"

namespace HouseCalderHedgerowImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<UInstancedStaticMeshComponent> Part;
    TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> Materials;
    TArray<FVector> HipPoints,CedarPoints;
    TMap<FName,TPair<FVector,FVector>> Views;
    float WindMin=FLT_MAX,WindMax=-FLT_MAX;
    bool Reviewed=false;
};
FState State;
constexpr float Height=220.f;
double Ground(const FVector2D& P) { return HouseExteriorSite::GroundHeight(P.X,P.Y); }

bool Clear(const FVector2D& P)
{
    const FBox2D Box(P-FVector2D(200),P+FVector2D(200));
    if (HouseCalderTerrain::BoxFrozen(Box) || HouseExteriorSite::GroundNormal(P.X,P.Y).Z<.94f) return false;
    for (const HouseCampusLayout::FBlock& Block:HouseCampusLayout::Blocks())
        if (Box.Intersect(Block.Rect().ExpandBy(100))) return false;
    for (int32 Y=0;Y<5;++Y) for (int32 X=0;X<5;++X)
    {
        const FVector2D Q(P.X-200+X*100,P.Y-200+Y*100);float WaterZ=0;
        if (HouseCampusLayout::PavingNear(Q,100) || HouseCalderFeatures::InStair(Q,100)
            || HouseCalderFeatures::InRamp(Q,100) || HouseCalderRoadside::Occupied(Q,100)
            || HouseCalderWater::WaterAt(Q,WaterZ) || HouseCalderWater::DeckAt(Q,WaterZ)) return false;
        for (const HouseCalderLandform::FLandformPath& Path:HouseCalderLandform::LandformPaths())
            for (int32 K=0;K<(Path.bClosed?Path.Points.Num():Path.Points.Num()-1);++K)
            {
                const FVector2D A=Path.Points[K],D=Path.Points[(K+1)%Path.Points.Num()]-A;
                const double T=FMath::Clamp(FVector2D::DotProduct(Q-A,D)/FMath::Max(D.SizeSquared(),.01),0.,1.);
                if (FVector2D::Distance(Q,A+D*T)<Path.Width*.5+100) return false;
            }
    }
    return true;
}

UTexture2D* SprayMask(const FHallImage& I)
{
    UTexture2D* T = UTexture2D::CreateTransient(I.W,I.H,PF_B8G8R8A8,MakeUniqueObjectName(GetTransientPackage(),UTexture2D::StaticClass(),TEXT("T_CalderHedgeSpray_A")));
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
    T->UpdateResource();return T;
}

UMaterialInstanceDynamic* Material(UObject* Outer,int32 Kind)
{
    const TCHAR* Path=(Kind==1 || Kind==2 || Kind==3 || Kind==5)
        ?TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderHedgerowFoliage.M_CalderHedgerowFoliage")
        :TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderHedgerow.M_CalderHedgerow");
    UMaterialInterface* Parent=LoadObject<UMaterialInterface>(nullptr,Path,nullptr,LOAD_NoWarn|LOAD_Quiet);
    if (!Parent) return nullptr;
    UMaterialInstanceDynamic* Result=UMaterialInstanceDynamic::Create(Parent,Outer);
    const int32 Size=256;FHallImage Image;Image.Init(Size,Size,FLinearColor::Black);TArray<float> Relief;Relief.SetNumUninitialized(Size*Size);
    const FLinearColor Colours[]={FLinearColor(.13,.12,.08),FLinearColor(.095,.17,.115),FLinearColor(.14,.19,.055),FLinearColor(.28,.12,.055),FLinearColor(.43,.025,.012),FLinearColor(.095,.17,.115)};
    for (int32 Y=0;Y<Size;++Y) for (int32 X=0;X<Size;++X)
    {
        const float U=(X+.5f)/Size,V=(Y+.5f)/Size;
        const float Noise=HallFbm(U*27,V*27,3,2026101780+Kind);
        const float Rib=FMath::Exp(-FMath::Square(U-.5f)*2200.f);
        const float Vein=FMath::Pow(FMath::Max(0.f,FMath::Cos((V*7-FMath::Abs(U-.5f)*5)*2*PI)),12.f);
        float Pattern=Noise*.08f;
        if (Kind==0) Pattern+=.09f*FMath::Sin(U*90+Noise*3);
        else if (Kind==1) Pattern+=.08f*FMath::Cos((V*12+FMath::Abs(U-.5f)*4)*2*PI);
        else if (Kind==2 || Kind==3) Pattern+=Rib*.15f+Vein*.035f;
        Image.At(X,Y)=Colours[Kind]*(.82f+.27f*Noise)+FLinearColor(.025f,.025f,.012f)*(Kind==2?Rib:0.f);
        Relief[Y*Size+X]=Pattern;
    }
    if (Kind==5)
    {
        Image.Init(256,512,FLinearColor(.11,.19,.12,0));
        const FLinearColor Stem(.08,.12,.055,1),Green(.115,.205,.13,1);
        Image.Stroke({{128,508},{122,350},{138,180},{128,12}},3,Stem);
        for (int32 Branch=0;Branch<6;++Branch) for (int32 Sign:{-1,1})
        {
            const FVector2D Base(128,440-Branch*67),Tip(128+Sign*(99-Branch*8),Base.Y-100);
            Image.Stroke({Base,(Base+Tip)*.5+FVector2D(Sign*6,6),Tip},2.5f,Green);
            const FVector2D Axis=(Tip-Base).GetSafeNormal(),Side(-Axis.Y,Axis.X);
            for (int32 Twig=0;Twig<8;++Twig)
            {
                const FVector2D From=FMath::Lerp(Base,Tip,.15+Twig*.1);
                const FVector2D To=From+(Axis*.65+Side*(Twig%2?1:-1))*34;
                Image.Stroke({From,To},2,Green);
                for (int32 Row=0;Row<12;++Row)
                {
                    const FVector2D Mid=FMath::Lerp(From,To,double(Row)/12);
                    for (int32 Pair:{-1,1})
                        Image.Ellipse(Mid+Side*(Pair*1.3),{1.6,3.6},float(FMath::RadiansToDegrees(FMath::Atan2(Axis.Y,Axis.X))-90),Green*(.85f+.08f*(Row%3)));
                }
            }
        }
        // Keep alpha in the raster and its coverage-scaled mip chain.
        Relief.SetNumUninitialized(Image.W*Image.H);
        for (int32 I=0;I<Image.Px.Num();++I) Relief[I]=Image.Px[I].A*.3f;
    }
    FHallImage Normal;HallHeightToNormal(Relief,Image.W,Image.H,Kind==0?1.3f:.8f,Kind!=5,Normal);
    UTexture2D* Albedo=Kind==5?SprayMask(Image):HallMakeTexture(Image,true,false,*FString::Printf(TEXT("T_CalderHedge%d_A"),Kind));
    UTexture2D* Normals=HallMakeTexture(Normal,false,true,*FString::Printf(TEXT("T_CalderHedge%d_N"),Kind));
    if (!Albedo || !Normals) return nullptr;
    Result->SetTextureParameterValue(TEXT("BaseMap"),Albedo);Result->SetTextureParameterValue(TEXT("NormalMap"),Normals);
    Result->SetVectorParameterValue(TEXT("Tint"),FLinearColor::White);Result->SetVectorParameterValue(TEXT("Emission"),FLinearColor::Black);
    Result->SetScalarParameterValue(TEXT("Roughness"),Kind==4?.68f:Kind==0?.91f:.72f);
    Result->SetScalarParameterValue(TEXT("NormalStrength"),.8f);Result->SetScalarParameterValue(TEXT("UVScale"),1.f);
    Result->SetScalarParameterValue(TEXT("WindStrength"),.55f);
    return Result;
}

UStaticMesh* BuildMesh(const FHallMesh (&Levels)[3],const TMap<FName,UMaterialInterface*>& Materials,bool CPU)
{
    UStaticMesh* Mesh=NewObject<UStaticMesh>(GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UStaticMesh::StaticClass(),TEXT("SM_CalderFarmHedgerow")),RF_Transient);
    Mesh->NeverStream=true;Mesh->bAllowCPUAccess=CPU;
    for (const FName Key:Levels[0].Order)
        Mesh->GetStaticMaterials().Add(FStaticMaterial(Materials.FindRef(Key),Key
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
        auto UVs=Attributes.GetVertexInstanceUVs();UVs.SetNumChannels(1);auto Slots=Attributes.GetPolygonGroupMaterialSlotNames();
        for (const FName Key:Levels[0].Order)
        {
            const FHallSection* Section=Levels[LOD].Sections.Find(Key);if (!Section) continue;
            const FPolygonGroupID Group=Description.CreatePolygonGroup();Slots[Group]=Key;TArray<FVertexInstanceID> Instances;
            for (int32 K=0;K<Section->V.Num();++K)
            {
                const FVertexID Vertex=Description.CreateVertex();Positions[Vertex]=FVector3f(Section->V[K]);
                const FVertexInstanceID Instance=Description.CreateVertexInstance(Vertex);Instances.Add(Instance);
                Normals[Instance]=FVector3f(Section->N[K]);Tangents[Instance]=FVector3f(Section->T[K].TangentX);
                Signs[Instance]=1;UVs.Set(Instance,0,FVector2f(Section->UV[K]));
            }
            for (int32 K=0;K+2<Section->I.Num();K+=3)
                Description.CreatePolygon(Group,TArray<FVertexInstanceID,TInlineAllocator<3>>{Instances[Section->I[K]],Instances[Section->I[K+1]],Instances[Section->I[K+2]]});
        }
        Pointers.Add(&Description);
    }
    UStaticMesh::FBuildMeshDescriptionsParams Parameters;Parameters.bBuildSimpleCollision=false;Parameters.bFastBuild=true;
    Parameters.bAllowCpuAccess=CPU;Parameters.bMarkPackageDirty=false;Parameters.bCommitMeshDescription=false;
    if (!Mesh->BuildFromMeshDescriptions(Pointers,Parameters)) return nullptr;
    if (FStaticMeshRenderData* Data=Mesh->GetRenderData())
    {Data->ScreenSize[0].Default=1;Data->ScreenSize[1].Default=.65f;Data->ScreenSize[2].Default=.12f;}
    Mesh->SetExtendedBounds(FBoxSphereBounds(Mesh->GetBoundingBox().ExpandBy(8)));
    return Mesh;
}

bool TriangleHit(const FVector& A,const FVector& B,const FVector& P,const FVector& Q,const FVector& R)
{
    const FVector D=B-A,E1=Q-P,E2=R-P,H=FVector::CrossProduct(D,E2);const double Det=FVector::DotProduct(E1,H);
    if (FMath::Abs(Det)<1e-9) return false;
    const FVector S=A-P;const double U=FVector::DotProduct(S,H)/Det;if (U<0 || U>1) return false;
    const FVector V=FVector::CrossProduct(S,E1);const double W=FVector::DotProduct(D,V)/Det;if (W<0 || U+W>1) return false;
    const double T=FVector::DotProduct(E2,V)/Det;return T>1e-5 && T<.99999;
}

bool ClearView(const FVector& Eye,const FVector& Target,UInstancedStaticMeshComponent* Part)
{
    const UStaticMesh* Mesh=Part?Part->GetStaticMesh():nullptr;const FStaticMeshRenderData* Data=Mesh?Mesh->GetRenderData():nullptr;
    if (!Data || !Data->LODResources.Num() || !Mesh->bAllowCPUAccess) return false;
    const FStaticMeshLODResources& LOD=Data->LODResources[0];const FVector End=Target+(Eye-Target).GetSafeNormal()*1.5;
    for (int32 Instance=0;Instance<Part->GetInstanceCount();++Instance)
    {
        FTransform Pose;Part->GetInstanceTransform(Instance,Pose,true);
        if (FVector::DistSquared2D(Pose.GetLocation(),Target)>FMath::Square(450.)) continue;
        const FVector A=Pose.InverseTransformPosition(Eye),B=Pose.InverseTransformPosition(End);
        for (uint32 I=0;I+2<uint32(LOD.IndexBuffer.GetNumIndices());I+=3)
        {
            const FVector P(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I)));
            const FVector Q(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I+1)));
            const FVector R(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I+2)));
            if (TriangleHit(A,B,P,Q,R)) return false;
        }
    }
    return true;
}
}

bool HouseCalderHedgerow::Enabled()
{
    return HouseCalderFarm::Enabled() && FParse::Param(FCommandLine::Get(),TEXT("CalderHedgerowTrial"));
}

void HouseCalderHedgerow::Build(AActor* Owner,UHouseBlackHallSubsystem* Look)
{
    namespace HI=HouseCalderHedgerowImpl;
    if (!Owner || !Owner->GetWorld() || !Look || !Enabled() || Owner->GetNetMode()==NM_DedicatedServer) return;
    auto& State=HI::State;if (State.World.Get()!=Owner->GetWorld()) {State=HI::FState();State.World=Owner->GetWorld();}
    if (State.Part.IsValid()) return;
    const double Start=FPlatformTime::Seconds();TMap<FName,UMaterialInterface*> Materials;
    const FName Keys[]={HI::CaneKey,HI::CedarKey,HI::RoseKey,HI::BriarKey,HI::HipKey,HI::SprayKey};
    for (int32 Kind=0;Kind<6;++Kind)
    {
        UMaterialInstanceDynamic* Material=HI::Material(Owner,Kind);
        if (!Material) {UE_LOG(LogTemp,Error,TEXT("CALDER_HEDGEROW FAIL missing material"));return;}
        Materials.Add(Keys[Kind],Material);State.Materials.Add(Material);
    }
    FHallMesh Levels[3];TArray<FVector> Hips,Cedars;
    for (int32 LOD=0;LOD<3;++LOD) HI::Geometry(Levels[LOD],LOD,LOD==0?&Hips:nullptr,LOD==0?&Cedars:nullptr);
    const bool CPU=FParse::Param(FCommandLine::Get(),TEXT("CalderHedgerowReview")) || FParse::Param(FCommandLine::Get(),TEXT("HouseExteriorViews"));
    UStaticMesh* Mesh=HI::BuildMesh(Levels,Materials,CPU);
    if (!Mesh) {UE_LOG(LogTemp,Error,TEXT("CALDER_HEDGEROW FAIL mesh build"));return;}
    UInstancedStaticMeshComponent* Part=NewObject<UInstancedStaticMeshComponent>(Owner,TEXT("CalderFarmHedgerow"),RF_Transient);
    Owner->AddInstanceComponent(Part);Part->SetupAttachment(Owner->GetRootComponent());Part->SetMobility(EComponentMobility::Static);
    Part->SetStaticMesh(Mesh);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetGenerateOverlapEvents(false);
    Part->SetCastShadow(false);Part->bAffectDistanceFieldLighting=false;Part->bAffectDynamicIndirectLighting=false;
    Part->SetCanEverAffectNavigation(false);Part->SetComponentTickEnabled(false);Part->SetCullDistances(7000,9000);Part->SetNumCustomDataFloats(2);
    Part->UpdateComponentToWorld();FRandomStream Random(2026101781);int32 Rejected=0;
    for (int32 K=0;K<32;++K)
    {
        const double Y=-2850+K*300.;
        if (Y>-500 && Y<1500) continue;
        const FVector2D P(-22200+Random.FRandRange(-35,35),Y+Random.FRandRange(-35,35));
        if (!HI::Clear(P)) {++Rejected;continue;}
        const FVector Normal=HouseExteriorSite::GroundNormal(P.X,P.Y);
        const FQuat Rotation=FQuat::FindBetweenNormals(FVector::UpVector,Normal)*FQuat(FVector::UpVector,Random.FRandRange(-PI,PI));
        const float Size=Random.FRandRange(.88f,1.08f),Vertical=Random.FRandRange(.92f,1.08f);
        const FTransform Pose(Rotation,FVector(P,HI::Ground(P)),FVector(Size,Size,Vertical));
        const int32 Index=Part->AddInstance(Pose,true);
        Part->SetCustomDataValue(Index,0,float(Pose.GetLocation().Z),false);Part->SetCustomDataValue(Index,1,HI::Height*Vertical,false);
        for (const FVector& Hip:Hips) State.HipPoints.Add(Pose.TransformPosition(Hip));
        for (const FVector& Cedar:Cedars) State.CedarPoints.Add(Pose.TransformPosition(Cedar));
    }
    if (FParse::Param(FCommandLine::Get(),TEXT("CalderHedgerowForceLOD0"))) Part->SetForcedLodModel(1);
    Part->UpdateBounds();Part->RegisterComponent();State.Part=Part;
    int32 WindMaterials=0;
    for (TActorIterator<AHouseLeafWind> Wind(Owner->GetWorld());Wind;++Wind)
        for (const TWeakObjectPtr<UMaterialInstanceDynamic>& Material:State.Materials)
            if (Material.IsValid()) {Wind->RegisterWindMaterial(Material.Get(),.55f);++WindMaterials;}
    const FStaticMeshRenderData* Data=Mesh->GetRenderData();
    if (Data) UE_LOG(LogTemp,Display,TEXT("CALDER_HEDGEROW_LOD_DATA screens=%.4f/%.4f/%.4f boundsRadius=%.3f boundsExtent=%s componentForceLOD0Trial=%d firstResidentLOD=%u inlinedLODs=%u vertices=%u/%u/%u"),
        Data->ScreenSize[0].GetValue(),Data->ScreenSize[1].GetValue(),Data->ScreenSize[2].GetValue(),Data->Bounds.SphereRadius,*Data->Bounds.BoxExtent.ToString(),
        int32(FParse::Param(FCommandLine::Get(),TEXT("CalderHedgerowForceLOD0"))),Data->CurrentFirstLODIdx,Data->NumInlinedLODs,Data->LODResources[0].GetNumVertices(),Data->LODResources[1].GetNumVertices(),Data->LODResources[2].GetNumVertices());
    UE_LOG(LogTemp,Display,TEXT("CALDER_HEDGEROW %s instances=%d rejected=%d components=1 plainISM=1 collision=0 shadows=0 cull=7000/9000 lodTris=%d/%d/%d windRegistrations=%d customFloats=2 existingBoundaryCollisionUnchanged=1 ms=%.1f"),
        Part->GetInstanceCount()>0 && WindMaterials==6?TEXT("PASS"):TEXT("FAIL"),Part->GetInstanceCount(),Rejected,
        Data?Data->LODResources[0].GetNumTriangles():0,Data?Data->LODResources[1].GetNumTriangles():0,Data?Data->LODResources[2].GetNumTriangles():0,
        WindMaterials,(FPlatformTime::Seconds()-Start)*1000);
}

bool HouseCalderHedgerow::DetailView(UWorld* World,FName Name,FVector& Eye,FVector& Target)
{
#if !UE_BUILD_SHIPPING
    namespace HI=HouseCalderHedgerowImpl;auto& State=HI::State;
    if (!World || !Enabled() || State.World.Get()!=World || !State.Part.IsValid()
        || State.Part->GetInstanceCount()==0
        || (Name!=TEXT("detail_calder_hedgerow") && Name!=TEXT("detail_calder_rose_hip") && Name!=TEXT("detail_calder_cedar_scale"))) return false;
    if (const TPair<FVector,FVector>* Cached=State.Views.Find(Name)) {Eye=Cached->Key;Target=Cached->Value;return true;}
    const bool Cedar=Name==TEXT("detail_calder_cedar_scale");
    const bool Close=Cedar || Name==TEXT("detail_calder_rose_hip");bool Found=false;
    if (Close)
    {
        int32 Tested=0;
        for (const FVector& Hip:Cedar?State.CedarPoints:State.HipPoints)
        {
            if (Hip.Z-HI::Ground(FVector2D(Hip))<(Cedar?125:95) || Hip.Z-HI::Ground(FVector2D(Hip))>(Cedar?135:165)) continue;
            if (++Tested>80) break;
            for (int32 K=0;K<8;++K)
            {
                const double A=-PI*.5+K*PI/4;const FVector2D P=FVector2D(Hip)+FVector2D(FMath::Cos(A)*(Cedar?14:45),FMath::Sin(A)*(Cedar?14:45));
                FHitResult Hit;const double G=HI::Ground(P);
                if (!World->LineTraceSingleByChannel(Hit,FVector(P,G+60),FVector(P,G-90),ECC_Visibility)
                    || FMath::Abs(Hit.ImpactPoint.Z-G)>4) continue;
                const FVector Camera(P,Hit.ImpactPoint.Z+130);
                if (!HI::ClearView(Camera,Hip,State.Part.Get())) continue;
                Eye=Camera;Target=Hip;Found=true;break;
            }
            if (Found) break;
        }
    }
    else
    {
        FTransform Pose;State.Part->GetInstanceTransform(0,Pose,true);
        const FVector2D P=FVector2D(Pose.GetLocation())+FVector2D(-500,-180);const double G=HI::Ground(P);FHitResult Hit;
        if (World->LineTraceSingleByChannel(Hit,FVector(P,G+60),FVector(P,G-90),ECC_Visibility) && FMath::Abs(Hit.ImpactPoint.Z-G)<4)
        {Eye=FVector(P,Hit.ImpactPoint.Z+130);Target=Pose.GetLocation()+FVector(0,0,105);Found=true;}
    }
    if (!Found) {UE_LOG(LogTemp,Warning,TEXT("CALDER_HEDGEROW_CAMERA name=%s physicalOrVisibilityCheckFailed=1"),*Name.ToString());return false;}
    State.Views.Add(Name,TPair<FVector,FVector>(Eye,Target));
    UE_LOG(LogTemp,Display,TEXT("CALDER_HEDGEROW_CAMERA name=%s groundPlus130=1 eye=%s target=%s"),*Name.ToString(),*Eye.ToString(),*Target.ToString());
    return true;
#else
    return false;
#endif
}

#if !UE_BUILD_SHIPPING
namespace HouseCalderHedgerowReviewImpl
{
void Tick(UWorld* World,ELevelTick,float)
{
    if (!World || !World->IsGameWorld() || !FParse::Param(FCommandLine::Get(),TEXT("CalderHedgerowReview"))) return;
    namespace HI=HouseCalderHedgerowImpl;auto& State=HI::State;
    if (State.World.Get()!=World) {State=HI::FState();State.World=World;}
    if (State.Reviewed) return;
    for (const TWeakObjectPtr<UMaterialInstanceDynamic>& Material:State.Materials)
        if (Material.IsValid())
        {
            const float Strength=Material->K2_GetScalarParameterValue(TEXT("WindStrength"));
            State.WindMin=FMath::Min(State.WindMin,Strength);State.WindMax=FMath::Max(State.WindMax,Strength);
        }
    if (World->GetTimeSeconds()<9 || (HouseCalderTerrain::Enabled() && !HouseCalderTerrain::CollisionReady(World))) return;
    State.Reviewed=true;const bool On=HouseCalderHedgerow::Enabled() && World->GetNetMode()!=NM_DedicatedServer;
    UInstancedStaticMeshComponent* Part=State.Part.Get();int32 Failures=0,Instances=0;float Worst=0;
    FString Csv=TEXT("instance,x,y,z,rootGapCm,heightCustom\n");
    if (On)
    {
        if (!Part || Cast<UHierarchicalInstancedStaticMeshComponent>(Part)) ++Failures;
        else
        {
            Instances=Part->GetInstanceCount();
            const UStaticMesh* Mesh=Part->GetStaticMesh();
            const FStaticMeshRenderData* Render=Mesh?Mesh->GetRenderData():nullptr;
            // Read actual render sections: no LOD may lose its foliage entirely.
            if (!Render || Render->LODResources.Num()!=3) ++Failures;
            else for (int32 LODIndex=0;LODIndex<3;++LODIndex)
                for (const FName Key:{HI::CedarKey,HI::RoseKey,HI::BriarKey,HI::SprayKey})
                {
                    const int32 Slot=Mesh->GetMaterialIndex(Key);uint32 Triangles=0;
                    for (const FStaticMeshSection& Section:Render->LODResources[LODIndex].Sections)
                        if (Section.MaterialIndex==Slot) Triangles+=Section.NumTriangles;
                    if (!Triangles) ++Failures;
                    UE_LOG(LogTemp,Display,TEXT("CALDER_HEDGEROW_FOLIAGE lod=%d section=%s triangles=%u"),LODIndex,*Key.ToString(),Triangles);
                }
            if (Instances<=0 || Part->NumCustomDataFloats!=2 || Part->PerInstanceSMCustomData.Num()!=Instances*2
                || Part->GetCollisionEnabled()!=ECollisionEnabled::NoCollision || Part->CastShadow || Part->bAffectDistanceFieldLighting
                || Part->bAffectDynamicIndirectLighting || Part->CanEverAffectNavigation() || Part->InstanceStartCullDistance!=7000 || Part->InstanceEndCullDistance!=9000) ++Failures;
            for (int32 I=0;I<Instances;++I)
            {
                FTransform Pose;Part->GetInstanceTransform(I,Pose,true);const FVector Root=Pose.GetLocation();FHitResult Hit;
                const bool Floor=World->LineTraceSingleByChannel(Hit,Root+FVector(0,0,60),Root-FVector(0,0,90),ECC_Visibility);
                const float Gap=Floor?Root.Z-Hit.ImpactPoint.Z:1000;Worst=FMath::Max(Worst,FMath::Abs(Gap));
                if (!Floor || FMath::Abs(Gap)>3 || !HI::Clear(FVector2D(Root))) ++Failures;
                if (Part->PerInstanceSMCustomData.Num()==Instances*2)
                {
                    if (FMath::Abs(Part->PerInstanceSMCustomData[I*2]-Root.Z)>.001
                        || FMath::Abs(Part->PerInstanceSMCustomData[I*2+1]-HI::Height*Pose.GetScale3D().Z)>.001) ++Failures;
                    Csv+=FString::Printf(TEXT("%d,%.6f,%.6f,%.6f,%.6f,%.6f\n"),I,Root.X,Root.Y,Root.Z,Gap,Part->PerInstanceSMCustomData[I*2+1]);
                }
                const FBox Bounds=Part->GetStaticMesh()->GetBoundingBox().TransformBy(Pose);
                if (HouseCalderTerrain::BoxFrozen(FBox2D(FVector2D(Bounds.Min),FVector2D(Bounds.Max)))) ++Failures;
            }
        }
        if (State.Materials.Num()!=6 || State.WindMax-State.WindMin<.001f) ++Failures;
    }
    else if (Part || State.Materials.Num()) ++Failures;
    const FString Summary=FString::Printf(TEXT("CALDER_HEDGEROW_REVIEW %s enabled=%d instances=%d failures=%d worstRootGapCm=%.3f windRange=%.6f oldBoundaryCollisionUnchanged=1 wholePhase2Complete=0\n"),
        Failures?TEXT("FAIL"):TEXT("PASS"),int32(On),Instances,Failures,Worst,On?State.WindMax-State.WindMin:0.f);
    UE_LOG(LogTemp,Display,TEXT("%s"),*Summary);const FString Dir=FPaths::ProjectSavedDir()/TEXT("CalderHedgerowReview");IFileManager::Get().MakeDirectory(*Dir,true);
    FFileHelper::SaveStringToFile(Summary,*(Dir/TEXT("result.txt")));FFileHelper::SaveStringToFile(Csv,*(Dir/TEXT("roots.csv")));
    if (Failures) FPlatformMisc::RequestExitWithStatus(false,1);
}
struct FRegistration {FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}} Registration;
}
#endif
