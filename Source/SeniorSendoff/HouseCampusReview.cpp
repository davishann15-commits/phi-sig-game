// Opt-in scene acceptance for the generated campus/woods; never changes normal gameplay.
#include "StoryCampaign.h"
#if !UE_BUILD_SHIPPING
#include "HouseExterior.h"
#include "HouseExteriorCampus.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseCalderTerrain.h"
#include "HouseBlackHall.h"
#include "HouseBasketball.h"
#include "HouseBasketballHoop.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
void TickHouseCampusReview(UWorld* W)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("CampusWoodsReview"))||!W||UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House")||W->GetTimeSeconds()<8||!HouseCalderTerrain::CollisionReady(W))return;
 static TWeakObjectPtr<UWorld> Done;if(Done.Get()==W)return;
 auto* Look=W->GetSubsystem<UHouseBlackHallSubsystem>();AHouseExteriorDetail* Exterior=nullptr;for(TActorIterator<AHouseExteriorDetail> It(W);It;++It)Exterior=*It;if(!Look||!Exterior)return;
 auto Mat=[&](const TCHAR* K){return Look->Mat(FName(K));};int NearVertices=0,SeatVertices=0,RidgeVertices[3]={0,0,0},RearBuildings=0,DistantTrees=0,CloseTrees=0;bool ShadowFree=true,GeometryValid=true;
 float NearMaxY=-MAX_flt,SeatsMaxY=-MAX_flt,RidgeMinY=MAX_flt;bool ForestCollisionFree=true;
 TInlineComponentArray<UProceduralMeshComponent*> Procedural(Exterior);
 for(auto* M:Procedural)
 {
  auto* PM=Cast<UProceduralMeshComponent>(M);const bool Near=PM->GetName().StartsWith(TEXT("ExteriorNear")),Far=PM->GetName()==TEXT("ExteriorFar");if(!Near&&!Far)continue;
  ShadowFree&=!PM->CastShadow;
  for(int J=0;J<PM->GetNumSections();++J){auto* Section=PM->GetProcMeshSection(J);if(!Section)continue;auto* Material=PM->GetMaterial(J);
   // The campus-side edge of the existing road is Y=-5550. Entrance
   // paving may approach it, but scenery must stay across that boundary.
   for(const auto& V:Section->ProcVertexBuffer){const FVector P=PM->GetComponentTransform().TransformPosition(V.Position);if(Near){++NearVertices;GeometryValid&=P.Y<-5550;NearMaxY=FMath::Max(NearMaxY,float(P.Y));}
    if(Material==Mat(TEXT("EX_CampusSeats"))){++SeatVertices;GeometryValid&=P.Y<-8000;SeatsMaxY=FMath::Max(SeatsMaxY,float(P.Y));}
    if(Far&&(Material==Mat(TEXT("EX_Brick"))||Material==Mat(TEXT("EX_CampusLimestone"))||Material==Mat(TEXT("EX_CampusWarmBrick")))&&P.Y>0)++RearBuildings;
    const TCHAR* Keys[]={TEXT("EX_RidgeNear"),TEXT("EX_RidgeMid"),TEXT("EX_RidgeFar")};for(int R=0;R<3;++R)if(Material==Mat(Keys[R])){++RidgeVertices[R];GeometryValid&=P.Y>30000;RidgeMinY=FMath::Min(RidgeMinY,float(P.Y));}
   }
  }
 }
 TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Instanced(Exterior);
 for(auto* C:Instanced){auto* H=Cast<UHierarchicalInstancedStaticMeshComponent>(C);if(H->GetName().StartsWith(TEXT("ExteriorForestDistant"))){DistantTrees+=H->GetInstanceCount();ShadowFree&=!H->CastShadow;ForestCollisionFree&=H->GetCollisionEnabled()==ECollisionEnabled::NoCollision;GeometryValid&=ForestCollisionFree;}
  if(H->GetName().StartsWith(TEXT("ExteriorTree")))for(int I=0;I<H->GetInstanceCount();++I){FTransform T;H->GetInstanceTransform(I,T,true);const FVector P=T.GetLocation();if(P.Y>4400&&P.Y<10100&&FMath::Abs(P.X)<7900)++CloseTrees;}
 }
 int Hoops=0,Balls=0;for(TActorIterator<AHouseBasketballHoop> It(W);It;++It)++Hoops;for(TActorIterator<AHouseBasketball> It(W);It;++It)++Balls;
 bool Floor=true;FCollisionQueryParams Q(SCENE_QUERY_STAT(CampusCourtFloor),true);if(auto* PC=W->GetFirstPlayerController())Q.AddIgnoredActor(PC->GetPawn());FHitResult Hit;
 for(float X:{-1000.f,0.f,1000.f})for(float Y:{2050.f,2750.f,3400.f})Floor&=W->LineTraceSingleByChannel(Hit,FVector(X,Y,-320),FVector(X,Y,-420),ECC_Pawn,Q)&&Hit.ImpactNormal.Z>.8f;
 if(FParse::Param(FCommandLine::Get(),TEXT("SurroundingsPolishReview")))
 {
  int StreetVertices=0,CarTypes=0,CarInstances=0;bool Valid=true,DecorationSafe=true,CarCollisionUnchanged=true;TSet<const UStaticMesh*> CarModels;
  for(auto* PM:Procedural)if(PM->GetName().StartsWith(TEXT("ExteriorStreetDecor")))
  {
   DecorationSafe&=!PM->CastShadow&&!PM->bAffectDynamicIndirectLighting&&!PM->bAffectDistanceFieldLighting&&PM->GetCollisionEnabled()==ECollisionEnabled::NoCollision;
   for(int J=0;J<PM->GetNumSections();++J)if(auto* S=PM->GetProcMeshSection(J))for(const auto& V:S->ProcVertexBuffer)
   {++StreetVertices;const FVector P=PM->GetComponentTransform().TransformPosition(V.Position);const bool FrontStreet=P.Y<-2700,SideStreet=P.X>8400,RearParking=P.X>3450&&P.Y>1900,WestParking=P.X<-3700&&P.Y<-300;Valid&=FrontStreet||SideStreet||RearParking||WestParking;}
  }
  // Classic cars (HouseClassicCars): six shared models in paint-scheme groups; each body box is sized like a real car.
  for(auto* H:Instanced)if(H->GetName().StartsWith(TEXT("ExteriorCar")))
  {
   ++CarTypes;CarInstances+=H->GetInstanceCount();const UStaticMesh* SM=H->GetStaticMesh();const auto* Body=SM?SM->GetBodySetup():nullptr;CarModels.Add(SM);
   CarCollisionUnchanged&=Body&&Body->AggGeom.BoxElems.Num()>=2;
   if(Body&&Body->AggGeom.BoxElems.Num()>=2){const auto& B=Body->AggGeom.BoxElems[0];CarCollisionUnchanged&=B.X>400.f&&B.X<600.f&&B.Y>150.f&&B.Y<205.f;}
  }
  const TCHAR* Keys[]={TEXT("EX_StreetAsphalt"),TEXT("EX_FratRoof"),TEXT("EX_DetailStone"),TEXT("EX_CarSeat"),TEXT("EX_CarPlate0")};
  for(const auto* K:Keys)Valid&=Look->Materials.Contains(K);
  int RoadFloors=0;for(const FVector P:{FVector(-7500,-4800,-60),FVector(-3000,-4800,-60),FVector(3000,-4800,-60),FVector(8200,-4900,-60),FVector(9550,500,-140)})
  {FHitResult H;RoadFloors+=int(W->LineTraceSingleByChannel(H,P+FVector(0,0,70),P-FVector(0,0,120),ECC_Pawn,Q)&&H.ImpactNormal.Z>.8f);}
  Valid&=StreetVertices>5000&&CarModels.Num()>=5&&CarInstances>10&&RoadFloors==5&&DecorationSafe&&CarCollisionUnchanged;
  const FString Result=FString::Printf(TEXT("%s streetVertices=%d classicCarModels=%d paintGroups=%d parkedCars=%d carColliders=%d roadFloors=%d/5 newDecorationCollisionAndShadowsOff=%d"),Valid?TEXT("PASS"):TEXT("FAIL"),StreetVertices,CarModels.Num(),CarTypes,CarInstances,int(CarCollisionUnchanged),RoadFloors,int(DecorationSafe));
  UE_LOG(LogTemp,Display,TEXT("SURROUNDINGS_POLISH_REVIEW %s"),*Result);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("SurroundingsPolishReview");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(Result,*(Dir/TEXT("result.txt")));
  if(!Valid){Done=W;FPlatformMisc::RequestExitWithStatus(false,1);return;}
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("FraternityBoundaryReview")))
 {
  int RowVertices=0,Edges=0,Crossings=0,Blocked=0,InsideClear=0,CameraClear=0,VisibilityClear=0;bool Valid=true;
  // The boundary is switched off for free roaming (HouseExteriorSiteLayout.h); then this only checks that none was built.
  const bool Boundary=HouseExteriorSite::PropertyBoundaryEnabled();
  for(auto* PM:Procedural)if(PM->GetName().StartsWith(TEXT("ExteriorFraternityRow")))
  {
   Valid&=!PM->CastShadow&&!PM->bAffectDynamicIndirectLighting&&PM->GetCollisionEnabled()==ECollisionEnabled::NoCollision;
   for(int J=0;J<PM->GetNumSections();++J)if(auto* Section=PM->GetProcMeshSection(J))for(const auto& V:Section->ProcVertexBuffer)
   {const FVector P=PM->GetComponentTransform().TransformPosition(V.Position);++RowVertices;Valid&=P.X<HouseExteriorSite::PlayableWestX-HouseExteriorSite::BoundaryHalfThickness&&P.Y<0&&P.Y>-4150;}
  }
  // Probe the real generated collision, isolating this perimeter from terrain,
  // trees and furniture so a scenery hit cannot masquerade as a boundary hit.
  FCollisionQueryParams BoundaryQ(SCENE_QUERY_STAT(FraternityBoundary),false);
  for(TActorIterator<AActor> It(W);It;++It)if(*It!=Exterior)BoundaryQ.AddIgnoredActor(*It);
  TInlineComponentArray<UPrimitiveComponent*> Primitives(Exterior);
  TArray<UBoxComponent*> EdgeBoxes;
  for(auto* P:Primitives)if(P->GetName().StartsWith(TEXT("PropertyBoundary_"))){if(auto* B=Cast<UBoxComponent>(P))EdgeBoxes.Add(B);}else BoundaryQ.AddIgnoredComponent(P);
  const auto* Pawn=Cast<AStoryFirstPersonCharacter>(W->GetFirstPlayerController()->GetPawn());
  const auto* Capsule=Pawn?Pawn->GetCapsuleComponent():nullptr;
  const auto Shape=FCollisionShape::MakeCapsule(Capsule?Capsule->GetScaledCapsuleRadius():36.f,Capsule?Capsule->GetScaledCapsuleHalfHeight():96.f);
  for(auto* B:EdgeBoxes)
  {
   ++Edges;const bool AlongY=B->GetName().Contains(TEXT("West"))||B->GetName().Contains(TEXT("East"));
   const float Sign=B->GetName().Contains(TEXT("West"))||B->GetName().Contains(TEXT("Front"))?1.f:-1.f;
   const FVector In=AlongY?FVector(Sign,0,0):FVector(0,Sign,0);const FBox Bounds=B->Bounds.GetBox();
   const float Expected=AlongY?(Sign>0?HouseExteriorSite::PlayableWestX:HouseExteriorSite::PlayableEastX):(Sign>0?HouseExteriorSite::PlayableFrontY:HouseExteriorSite::PlayableRearY);
   Valid&=FMath::IsNearlyEqual(AlongY?B->GetComponentLocation().X:B->GetComponentLocation().Y,double(Expected),1.0);
   Valid&=B->GetCollisionEnabled()==ECollisionEnabled::QueryOnly&&B->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block&&B->GetCollisionResponseToChannel(ECC_Camera)==ECR_Ignore&&B->GetCollisionResponseToChannel(ECC_Visibility)==ECR_Ignore&&B->bHiddenInGame;
   for(float U:{.08f,.29f,.50f,.71f,.92f})
   {
    FVector P=B->GetComponentLocation();P.Z=150;
    if(AlongY)P.Y=FMath::Lerp(Bounds.Min.Y,Bounds.Max.Y,U);else P.X=FMath::Lerp(Bounds.Min.X,Bounds.Max.X,U);
    const FVector A=P+In*360,Beyond=P-In*360;FHitResult H;
    Blocked+=int(W->SweepSingleByChannel(H,A,Beyond,FQuat::Identity,ECC_Pawn,Shape,BoundaryQ)&&H.GetComponent()==B);
    InsideClear+=int(!W->SweepSingleByChannel(H,A,A+In*150,FQuat::Identity,ECC_Pawn,Shape,BoundaryQ));
    CameraClear+=int(!W->LineTraceSingleByChannel(H,A,Beyond,ECC_Camera,BoundaryQ));
    VisibilityClear+=int(!W->LineTraceSingleByChannel(H,A,Beyond,ECC_Visibility,BoundaryQ));++Crossings;
   }
  }
  // Diagonal corner sweeps must also meet a boundary rather than find a seam.
  if(Boundary)for(float X:{HouseExteriorSite::PlayableWestX,HouseExteriorSite::PlayableEastX})for(float Y:{HouseExteriorSite::PlayableFrontY,HouseExteriorSite::PlayableRearY})
  {const FVector In(X<0?1.f:-1.f,Y<0?1.f:-1.f,0);const FVector P(X,Y,150);FHitResult H;Blocked+=int(W->SweepSingleByChannel(H,P+In*300,P-In*300,FQuat::Identity,ECC_Pawn,Shape,BoundaryQ));++Crossings;}
  Valid&=Boundary?RowVertices>10000&&Edges==4&&Crossings==24&&Blocked==24&&InsideClear==20&&CameraClear==20&&VisibilityClear==20:RowVertices>10000&&Edges==0;
  const FString Detail=FString::Printf(TEXT("%s boundary=%s rowVertices=%d boundaryBoxes=%d blockedCrossings=%d/%d insideClear=%d/20 cameraClear=%d/20 visibilityClear=%d/20"),Valid?TEXT("PASS"):TEXT("FAIL"),Boundary?TEXT("on"):TEXT("off"),RowVertices,Edges,Blocked,Crossings,InsideClear,CameraClear,VisibilityClear);
  UE_LOG(LogTemp,Display,TEXT("FRATERNITY_BOUNDARY_REVIEW %s"),*Detail);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("FraternityBoundaryReview");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(Detail,*(Dir/TEXT("result.txt")));
  if(!Valid){Done=W;FPlatformMisc::RequestExitWithStatus(false,1);return;}
 }
 const bool OK=GeometryValid&&ShadowFree&&RearBuildings==0&&NearVertices>15000&&SeatVertices>1000&&RidgeVertices[0]>1000&&RidgeVertices[1]>1000&&RidgeVertices[2]>1000&&DistantTrees>600&&CloseTrees>=30&&Hoops==2&&Balls==3&&Floor;
 UE_LOG(LogTemp,Display,TEXT("CAMPUS_REGIONS nearMaxY=%.1f seatsMaxY=%.1f ridgeMinY=%.1f forestCollisionFree=%d"),NearMaxY,SeatsMaxY,RidgeMinY,int(ForestCollisionFree));
 const FString Result=FString::Printf(TEXT("%s campusFrontVertices=%d stadiumSeatVertices=%d rearBuildingVertices=%d ridgeVertices=%d,%d,%d closeRearTrees=%d distantRearTrees=%d distantShadowsOff=%d geometryRegionsValid=%d courtFloorSolid=%d hoops=%d basketballs=%d\n"),OK?TEXT("PASS"):TEXT("FAIL"),NearVertices,SeatVertices,RearBuildings,RidgeVertices[0],RidgeVertices[1],RidgeVertices[2],CloseTrees,DistantTrees,int(ShadowFree),int(GeometryValid),int(Floor),Hoops,Balls);
 FString Dir=FPaths::ProjectSavedDir()/TEXT("CampusWoodsReview");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(Result,*(Dir/TEXT("result.txt")));UE_LOG(LogTemp,Display,TEXT("CAMPUS_WOODS_REVIEW %s"),*Result);Done=W;if(!OK)FPlatformMisc::RequestExitWithStatus(false,1);
}
#else
void TickHouseCampusReview(UWorld*){}
#endif
