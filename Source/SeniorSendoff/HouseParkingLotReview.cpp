#include "HouseFraternityRow.h"
#include "StoryCampaign.h"
#if !UE_BUILD_SHIPPING
#include "HouseParkingLot.h"
#include "HouseExterior.h"
#include "HouseBlackHall.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CoreDelegates.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"

void TickHouseParkingLotReview(UWorld* W)
{
 const bool Walk=FParse::Param(FCommandLine::Get(),TEXT("ParkingLotWalkReview")),Scene=FParse::Param(FCommandLine::Get(),TEXT("ParkingLotReview"));
 if((!Walk&&!Scene)||!W||UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House"))return;
 auto* PC=W->GetFirstPlayerController();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;if(!Pawn)return;
 struct FState{TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;int Phase=0,Leg=1;double Start=0,Step=0;FVector Goal;TArray<FVector2D> Route;FString Log,Dir;};static FState S;
 static const FDelegateHandle Input=FCoreDelegates::OnBeginFrame.AddLambda([](){if(S.Phase==1&&S.Pawn.IsValid())S.Pawn->AddMovementInput((S.Goal-S.Pawn->GetActorLocation()).GetSafeNormal2D(),1);});
 if(S.World.Get()!=W){S=FState();S.World=W;S.Pawn=Pawn;S.Start=S.Step=W->GetTimeSeconds();S.Dir=FPaths::ProjectSavedDir()/TEXT("ParkingLotReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
 auto Note=[&](const FString& T){S.Log+=T+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("PARKING_LOT_REVIEW %s"),*T);};
 auto Finish=[&](bool OK,const FString& T){Note((OK?TEXT("PASS "):TEXT("FAIL "))+T);FFileHelper::SaveStringToFile(S.Log,*(S.Dir/TEXT("result.txt")));S.Phase=-1;if(!OK||Walk)FPlatformMisc::RequestExitWithStatus(false,OK?0:1);};
 if(S.Phase<0)return;const double Now=W->GetTimeSeconds();const float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 if(Now-S.Start>90){Finish(false,TEXT("Parking route timed out"));return;}
 if(S.Phase==0)
 {
  if(Now-S.Start<8)return;AHouseExteriorDetail* E=nullptr;for(TActorIterator<AHouseExteriorDetail> It(W);It;++It)E=*It;if(!E)return;
  auto* L=W->GetSubsystem<UHouseBlackHallSubsystem>();int CenterPoles=0,LotCars=0,Plants=0,BlueVertices=0,SymbolVertices=0;bool Safe=true,ReservedClear=true;
  const FBox ProtectedAreas[]={FBox(FVector(-4880,-950,-80),FVector(-4480,-330,190)),FBox(FVector(-4160,-950,-80),FVector(-3760,-330,190)),FBox(FVector(-4480,-950,-80),FVector(-4160,-300,190)),FBox(FVector(-5800,-2500,-80),FVector(-5200,-1850,190))};
  TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Parts(E);
  for(auto* H:Parts)
  {
   if(H->GetName().StartsWith(TEXT("ExteriorBushParking"))){Plants+=H->GetInstanceCount();Safe&=!H->CastShadow&&H->GetCollisionEnabled()==ECollisionEnabled::NoCollision&&!H->bAffectDynamicIndirectLighting;}
   if(H->GetName()==TEXT("ExteriorPole"))for(int I=0;I<H->GetInstanceCount();++I){FTransform T;H->GetInstanceTransform(I,T,true);const auto P=T.GetLocation();CenterPoles+=int(P.X>HouseParking::West&&P.X<HouseParking::East&&P.Y>HouseParking::South&&P.Y<HouseParking::North);}
   if(H->GetName().StartsWith(TEXT("ExteriorCar")))for(int I=0;I<H->GetInstanceCount();++I){FTransform T;H->GetInstanceTransform(I,T,true);const auto P=T.GetLocation();if(P.X>HouseParking::West&&P.X<HouseParking::East&&P.Y>HouseParking::South&&P.Y<HouseParking::North){++LotCars;const UStaticMesh* SM=H->GetStaticMesh();if(!SM){ReservedClear=false;continue;}const FBox B=SM->GetBoundingBox().TransformBy(T.ToMatrixWithScale());for(const FBox& Area:ProtectedAreas)ReservedClear&=!B.Intersect(Area);}}
  }
  TInlineComponentArray<UProceduralMeshComponent*> Meshes(E);
  for(auto* P:Meshes)if(P->GetName().StartsWith(TEXT("ExteriorParkingLot")))
  {
   Safe&=!P->CastShadow&&!P->bAffectDynamicIndirectLighting&&!P->bAffectDistanceFieldLighting;
   if(P->GetName().Contains(TEXT("Detail")))Safe&=P->GetCollisionEnabled()==ECollisionEnabled::NoCollision;
   for(int I=0;I<P->GetNumSections();++I)if(auto* Sec=P->GetProcMeshSection(I)){if(P->GetMaterial(I)==L->Mat(TEXT("EX_ParkingBlue")))BlueVertices+=Sec->ProcVertexBuffer.Num();if(P->GetMaterial(I)==L->Mat(TEXT("EX_ParkingSymbol")))SymbolVertices+=Sec->ProcVertexBuffer.Num();}
  }
  FCollisionQueryParams Q(SCENE_QUERY_STAT(ParkingLotFloor),true,Pawn);FHitResult H;int Floors=0,Paths=0,Curbs=0;
  auto Supported=[&](FVector P){return W->LineTraceSingleByChannel(H,P+FVector(0,0,70),P-FVector(0,0,70),ECC_Pawn,Q)&&FMath::Abs(H.ImpactPoint.Z-P.Z)<3&&H.ImpactNormal.Z>.8f;};
  for(const FVector P:{FVector(-5510,-2490,-58),FVector(-5500,-1400,-58),FVector(-4320,-700,-58),FVector(-4680,-650,-58),FVector(-3960,-650,-58),FVector(-3700,-195,-57),FVector(-3480,-195,-57)})Floors+=int(Supported(P));
  for(const FVector P:{FVector(-7188,-1400,-43),FVector(-3711,-1250,-43)})Curbs+=int(Supported(P));
  const auto Capsule=FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half);const float Z=HouseParking::Surface+Half+3;
  const TPair<FVector,FVector> Routes[]={{FVector(-5510,-2900,Z),FVector(-5510,-1350,Z)},{FVector(-5510,-1400,Z),FVector(-4320,-1400,Z)},{FVector(-4320,-1400,Z),FVector(-4320,-195,Z)},{FVector(-4320,-195,Z),FVector(-3480,-195,Z)}};
  for(const auto& Route:Routes)Paths+=int(!W->SweepSingleByChannel(H,Route.Key,Route.Value,FQuat::Identity,ECC_Pawn,Capsule,Q));
  Note(FString::Printf(TEXT("centerLampPosts=%d parkedCars=%d reservedBaysAndAislesClear=%d blueBays=%d symbols=%d landscaping=%d floorSamples=%d/7 raisedCurbs=%d/2 clearCapsuleRoutes=%d/4 lowCostDecoration=%d exteriorLights=%d"),CenterPoles,LotCars,int(ReservedClear),BlueVertices/4,SymbolVertices/4,Plants,Floors,Curbs,Paths,int(Safe),E->Lights));
  if(HouseFraternityRow::Enabled())
  {
   int RemovedPoles=0;for(auto* P:Parts)if(P->GetName().StartsWith(TEXT("ExteriorPoleLot")))for(int I=0;I<P->GetInstanceCount();++I){FTransform T;P->GetInstanceTransform(I,T,true);for(FVector2D XY:{FVector2D(-7160,-2600),FVector2D(-3740,-2530)})RemovedPoles+=FVector2D::Distance(FVector2D(T.GetLocation()),XY)<1;}
   int RemovedParts=0;for(auto* P:Meshes)if(P->GetName().StartsWith(TEXT("ExteriorParkingLot"))||P->GetName().StartsWith(TEXT("ExteriorLotProps")))++RemovedParts;
   if(CenterPoles!=0||LotCars!=0||BlueVertices!=0||SymbolVertices!=0||Plants!=0||Floors!=7||Curbs!=0||Paths!=4||!Safe||RemovedParts!=0||RemovedPoles!=0||E->Lights!=HouseFraternityRow::ExteriorLightBudget())
   {Finish(false,TEXT("Reclaimed Row lawn must have zero parkingcars/bays/curbs/props, supported ground and all original pedestrian routes"));return;}
   Note(TEXT("ReclaimedRow westLotAbsent=1 oldWalkWaypointsUnchanged=1 mainHouseConnectorSupported=1"));
  }
  else if(CenterPoles!=0||LotCars<3||!ReservedClear||BlueVertices!=8||SymbolVertices!=8||Plants<25||Floors!=7||Curbs!=2||Paths!=4||!Safe||E->Lights>53){Finish(false,TEXT("Actual parking layout checks failed"));return;}
  if(!Walk){Finish(true,TEXT("Parking bays, accessible aisle, driveway and connection to the house are clear and supported"));return;}
  S.Route={{-5510,-2800},{-5510,-1400},{-4320,-1400},{-4320,-600},{-4320,-195},{-3480,-195}};S.Step=Now;Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->SetActorLocation({S.Route[0].X,S.Route[0].Y,Z},false,nullptr,ETeleportType::TeleportPhysics);Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);S.Goal=FVector(S.Route[1],0);S.Phase=1;return;
 }
 if(S.Phase==1)
 {
  const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Half);PC->SetControlRotation((S.Goal-Feet).Rotation());
  if(Now-S.Step>15||Feet.Z<-90){Finish(false,TEXT("Walk stalled or dropped at ")+Feet.ToString());return;}
  if(FVector::Dist2D(Feet,S.Goal)<40){Pawn->GetCharacterMovement()->StopMovementImmediately();Note(FString::Printf(TEXT("Walking leg %d reached %s"),S.Leg,*Feet.ToString()));++S.Leg;S.Step=Now;if(S.Leg>=S.Route.Num()){FScreenshotRequest::RequestScreenshot(S.Dir/TEXT("ParkingToHouseWalk.png"),false,false);S.Phase=2;return;}S.Goal=FVector(S.Route[S.Leg],0);}
 }
 if(S.Phase==2&&Now-S.Step>1)Finish(true,TEXT("Player walked from the driveway through the parking aisle and accessible walkway to the house without a collision stall. Single-player check."));
}
#else
void TickHouseParkingLotReview(UWorld*){}
#endif
