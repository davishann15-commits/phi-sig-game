#include "HouseStadiumDetail.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallPaint.h"
#include "HouseCampusFall.h"
#include "HouseCampusPolish.h"
#include "HouseCampusStaticMesh.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
namespace HouseStadiumDetail
{
bool Enabled(){return HouseCampusFall::Enabled()&&HouseCampusPolish::Enabled()&&!FParse::Param(FCommandLine::Get(),TEXT("NoStadiumDetail"));}
int32 Step(){int32 Value=1;
#if !UE_BUILD_SHIPPING
 FParse::Value(FCommandLine::Get(),TEXT("StadiumDetailStep="),Value);
#endif
 return FMath::Clamp(Value,1,7);}
bool Aisle(int32 K){return Enabled()&&(K==0||K==2||K==17||K==19||K==20||K==22||K==37||K==39);}
bool Portal(int32 K){return Enabled()&&(K==4||K==16);}
bool PortalBand(int32 K,int32 Row){return Portal(K)&&Row>=4&&Row<=7;}
bool CrossAisle(int32 K,int32 Row){return Enabled()&&Row==3&&((K>=2&&K<=4)||(K>=16&&K<=17));}
}
AHouseStadiumDetail::AHouseStadiumDetail()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.1f;bReplicates=false;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("CalderStadiumRoot")));RootComponent->SetMobility(EComponentMobility::Static);}
void AHouseStadiumDetail::SpawnFor(AActor* Exterior,UHouseBlackHallSubsystem* InLook)
{
 if(!HouseStadiumDetail::Enabled()||!Exterior||!InLook)return;
 for(TActorIterator<AHouseStadiumDetail> It(Exterior->GetWorld());It;++It)if(IsValid(*It))return;
 const double Start=FPlatformTime::Seconds();FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;P.Name=TEXT("CalderStadiumDetail");P.Owner=Exterior;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* A=Exterior->GetWorld()->SpawnActor<AHouseStadiumDetail>(StaticClass(),FTransform::Identity,P);if(!A)return;
 A->Look=InLook;HouseStadiumDetail::CreateLook(InLook);A->BoxKit=HouseStadiumDetail::Kit(InLook,false);A->PipeKit=HouseStadiumDetail::Kit(InLook,true);A->BenchKit=HouseStadiumDetail::PlankKit(InLook);
 if(!A->BoxKit||!A->PipeKit||!A->BenchKit){A->Destroy();return;}
 A->Seating();if(HouseStadiumDetail::Step()>=2)A->Structures();if(HouseStadiumDetail::Step()>=3)A->Masts();if(HouseStadiumDetail::Step()>=4)A->Field();if(HouseStadiumDetail::Step()>=5)A->Concourse();
 A->FinishMesh(A->Unique,TEXT("CalderStadiumUnique"));A->Unique=FHallMesh();if(HouseStadiumDetail::Step()>=7)A->Mood();else A->SetActorTickEnabled(false);
 if(A->Collision.NumTriangles()){auto* C=NewObject<UProceduralMeshComponent>(A,TEXT("CalderStadiumStairCollision"));A->AddInstanceComponent(C);C->SetupAttachment(A->RootComponent);C->SetMobility(EComponentMobility::Static);C->SetCastShadow(false);C->bAffectDistanceFieldLighting=false;C->bAffectDynamicIndirectLighting=false;C->SetCanEverAffectNavigation(false);C->bUseComplexAsSimpleCollision=true;C->bUseAsyncCooking=false;C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);C->SetCollisionObjectType(ECC_WorldStatic);C->SetCollisionResponseToAllChannels(ECR_Block);C->SetGenerateOverlapEvents(false);C->RegisterComponent();A->Collision.Build(C,[InLook](FName K){return InLook->Mat(K);},true);A->Collision=FHallMesh();}
 for(auto& Pair:A->Batches)Pair.Value->BuildTreeIfOutdated(false,true);
 UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM built step=%d instances=%d batches=%d triangles=%d newLights=0 stadiumPoints=3 ms=%.3f"),HouseStadiumDetail::Step(),A->Instances,A->Batches.Num(),A->Triangles,(FPlatformTime::Seconds()-Start)*1000.);
}
UHierarchicalInstancedStaticMeshComponent* AHouseStadiumDetail::Batch(FName Material,int32 Shape,bool Solid,float Cull)
{
 const FName Name(*FString::Printf(TEXT("CalderStadium_%s_%d_%d_%d"),*Material.ToString(),Shape,int(Solid),int(Cull)));
 if(auto* Found=Batches.Find(Name))return *Found;
 auto* H=NewObject<UHierarchicalInstancedStaticMeshComponent>(this,Name);AddInstanceComponent(H);H->SetupAttachment(RootComponent);H->SetMobility(EComponentMobility::Static);H->SetStaticMesh(Shape==2?BenchKit:Shape==1?PipeKit:BoxKit);H->SetMaterial(0,Look->Mat(Material));H->SetCastShadow(false);H->bAffectDistanceFieldLighting=false;H->bAffectDynamicIndirectLighting=false;H->SetGenerateOverlapEvents(false);H->SetCanEverAffectNavigation(false);H->SetCollisionEnabled(Solid?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);H->SetCollisionObjectType(ECC_WorldStatic);H->SetCollisionResponseToAllChannels(ECR_Block);H->SetCullDistances(Cull*.8f,Cull);H->bAutoRebuildTreeOnInstanceChanges=false;H->RegisterComponent();Batches.Add(Name,H);return H;
}
void AHouseStadiumDetail::Box(FName Material,FVector Centre,FVector Dimensions,FRotator Rotation,bool Solid,float Cull)
{if(Dimensions.GetMin()<=0.f)return;Batch(Material,0,Solid,Cull)->AddInstance(FTransform(Rotation,Centre,Dimensions/100.f),false);++Instances;}
void AHouseStadiumDetail::Pipe(FName Material,FVector A,FVector B,float Radius,bool Solid,float Cull)
{const float Length=FVector::Dist(A,B);if(Length<.01f)return;Batch(Material,1,Solid,Cull)->AddInstance(FTransform(FRotationMatrix::MakeFromZ(B-A).ToQuat(),(A+B)*.5f,FVector(Radius/50.f,Radius/50.f,Length/100.f)),false);++Instances;}
void AHouseStadiumDetail::Plank(FName Material,FVector Centre,float Span,FRotator Rotation)
{Batch(Material,2,true,24000)->AddInstance(FTransform(Rotation,Centre,FVector(Span/175.f,1,1)),false);++Instances;}
void AHouseStadiumDetail::FinishMesh(const FHallMesh& Geometry,FName Name,float Cull)
{if(!Geometry.NumTriangles())return;auto* Mesh=HouseCampusStaticMesh::Build(Geometry,Name,Look);if(!Mesh)return;OwnedMeshes.Add(Mesh);auto* C=NewObject<UStaticMeshComponent>(this,Name);AddInstanceComponent(C);C->SetupAttachment(RootComponent);C->SetMobility(EComponentMobility::Static);C->SetStaticMesh(Mesh);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetGenerateOverlapEvents(false);C->SetCastShadow(false);C->bAffectDistanceFieldLighting=false;C->bAffectDynamicIndirectLighting=false;C->SetCanEverAffectNavigation(false);C->SetCullDistance(Cull);C->RegisterComponent();Triangles+=Geometry.NumTriangles();}
void AHouseStadiumDetail::Sign(FName Material,FVector C,FVector Right,FVector N,float Width,float Height)
{const FVector U=FVector::UpVector;Unique.QuadUV(Material,C-Right*Width*.5f-U*Height*.5f,C+Right*Width*.5f-U*Height*.5f,C+Right*Width*.5f+U*Height*.5f,C-Right*Width*.5f+U*Height*.5f,N,{0,1},{1,1},{1,0},{0,0},Right);}
void AHouseStadiumDetail::Letter(const FString& Word,FVector C,FVector Right,FVector N,float Height,FName Material)
{const float Pitch=Height*.85f;const FVector Left=C-Right*((Word.Len()-1)*Pitch*.5f+Height/3.f);for(int32 I=0;I<Word.Len();++I)for(const auto& S:HallGlyph(Word[I]))for(int32 J=1;J<S.Num();++J){auto V=[&](FVector2D P){return Left+Right*(I*Pitch+P.X*Height/6.f)+FVector(0,0,P.Y*Height/6.f-Height*.5f);};const FVector A=V(S[J-1]),B=V(S[J]),Side=FVector::CrossProduct(N,(B-A).GetSafeNormal())*(Height*.045f);Unique.Quad(Material,A-Side,B-Side,B+Side,A+Side,N,FHallUV::World(100));}}
void AHouseStadiumDetail::Rail(FVector A,FVector B,bool Middle)
{const int32 Posts=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/130.f));for(int32 I=0;I<=Posts;++I){const FVector P=FMath::Lerp(A,B,float(I)/Posts);Pipe(TEXT("SD_Steel"),P,P+FVector(0,0,110),2.7f);Box(TEXT("SD_Steel"),P+FVector(0,0,1),{12,12,2});}Pipe(TEXT("SD_Steel"),A+FVector(0,0,110),B+FVector(0,0,110),3.f);if(Middle)Pipe(TEXT("SD_Steel"),A+FVector(0,0,55),B+FVector(0,0,55),2.3f);}
