#include "StoryCampaign.h"
#if !UE_BUILD_SHIPPING
#include "HouseWoodsDetail.h"
#include "HouseExterior.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CoreDelegates.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"

void TickHouseWoodsReview(UWorld* W)
{
 const bool Scene=FParse::Param(FCommandLine::Get(),TEXT("WoodsSceneReview")),Walk=FParse::Param(FCommandLine::Get(),TEXT("WoodsWalkReview"));
 if((!Scene&&!Walk)||!W||UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House"))return;
 auto* PC=W->GetFirstPlayerController();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;if(!Pawn||!Pawn->HasAuthority())return;
 struct FState{TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;double Start=0,Step=0;int Phase=0,Leg=1;FVector Goal;TArray<FVector2D> Route;FString Log,Dir;};static FState S;
 static const FDelegateHandle Input=FCoreDelegates::OnBeginFrame.AddLambda([](){if(S.Pawn.IsValid()&&S.Phase==1)S.Pawn->AddMovementInput((S.Goal-S.Pawn->GetActorLocation()).GetSafeNormal2D(),1);});
 if(S.World.Get()!=W){S=FState();S.World=W;S.Pawn=Pawn;S.Start=S.Step=W->GetTimeSeconds();S.Dir=FPaths::ProjectSavedDir()/TEXT("WoodsDetailReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
 auto Note=[&](FString Text){S.Log+=Text+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("WOODS_REVIEW %s"),*Text);};
 auto Finish=[&](bool OK,FString Text){Note((OK?TEXT("PASS "):TEXT("FAIL "))+Text);FFileHelper::SaveStringToFile(S.Log,*(S.Dir/TEXT("result.txt")));S.Phase=-1;if(Walk||!OK)FPlatformMisc::RequestExitWithStatus(false,OK?0:1);};
 const double Now=W->GetTimeSeconds();if(S.Phase<0)return;if(Now-S.Start>160){Finish(false,TEXT("Route timed out"));return;}
 const float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 if(S.Phase==0)
 {
  if(Now-S.Start<8)return;
  AHouseExteriorDetail* Exterior=nullptr;for(TActorIterator<AHouseExteriorDetail> It(W);It;++It)Exterior=*It;if(!Exterior)return;
  TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Parts(Exterior);int Logs=0,Branches=0,Flowers=0,Rocks=0,Leaves=0,Shrubs=0,Grass=0;float Minimum=MAX_flt,Maximum=0;bool PropsOK=true;
  FCollisionQueryParams GroundQ(SCENE_QUERY_STAT(WoodsGround),true,Pawn);
  for(auto* H:Parts)
  {
   const FString Name=H->GetName();if(Name.StartsWith(TEXT("ExteriorWoods")))
   {
    if(Name.Contains(TEXT("FallenLog"))){Logs+=H->GetInstanceCount();GroundQ.AddIgnoredComponent(H);PropsOK&=H->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics;}
    if(Name.Contains(TEXT("Branches"))){Branches+=H->GetInstanceCount();PropsOK&=H->GetCollisionEnabled()==ECollisionEnabled::NoCollision;}
    if(Name.Contains(TEXT("Flowers"))){Flowers+=H->GetInstanceCount();PropsOK&=H->GetCollisionEnabled()==ECollisionEnabled::NoCollision;}
    if(Name.Contains(TEXT("Rock"))){Rocks+=H->GetInstanceCount();GroundQ.AddIgnoredComponent(H);}
    if(Name.Contains(TEXT("Leaves")))Leaves+=H->GetInstanceCount();
    if(Name.Contains(TEXT("Shrub")))Shrubs+=H->GetInstanceCount();
    if(Name.Contains(TEXT("Grass")))Grass+=H->GetInstanceCount();
    if(Name.Contains(TEXT("Shrub"))||Name.Contains(TEXT("Grass")))
    {
     PropsOK&=H->GetCollisionEnabled()==ECollisionEnabled::NoCollision&&!H->CastShadow&&!H->bAffectDistanceFieldLighting&&!H->bAffectDynamicIndirectLighting;
     // Verify the generated world-space foliage leaves the actual route and porch clear.
     for(int I=0;I<H->GetInstanceCount();++I){FTransform T;H->GetInstanceTransform(I,T,true);const auto P=T.GetLocation();PropsOK&=HouseWoods::TrailDistance(P.X,P.Y)>(Name.Contains(TEXT("Shrub"))?330.f:220.f)&&FVector2D((P.X-HouseWoods::HutX)/1.25f,P.Y-HouseWoods::HutY).Size()>610.f;}
    }
    if(H->GetInstanceCount()>0){FTransform T;H->GetInstanceTransform(0,T,true);Note(Name+TEXT(" first=")+T.GetLocation().ToString());}
   }
   if(Name.StartsWith(TEXT("ExteriorForestDistant")))for(int I=0;I<H->GetInstanceCount();++I){FTransform T;H->GetInstanceTransform(I,T,true);const float V=T.GetScale3D().Z;Minimum=FMath::Min(Minimum,V);Maximum=FMath::Max(Maximum,V);}
  }
  if(!PropsOK||Logs<15||Branches<120||Flowers<300||Rocks<70||Leaves<1800||Minimum>.7f||Maximum<1.7f){Finish(false,FString::Printf(TEXT("Prop/scale acceptance logs=%d branches=%d flowers=%d rocks=%d leaves=%d scale=%.2f..%.2f collisions=%d"),Logs,Branches,Flowers,Rocks,Leaves,Minimum,Maximum,int(PropsOK)));return;}
  if(FParse::Param(FCommandLine::Get(),TEXT("WoodsUndergrowthReview"))&&(Shrubs<300||Grass<8000)){Finish(false,TEXT("Dense undergrowth is missing from the actual world"));return;}
  Note(FString::Printf(TEXT("Undergrowth shrubs=%d grassClumps=%d; no decorative foliage collision, shadows or indirect-light updates; trail and clearing plant margins clear"),Shrubs,Grass));
  int Samples=0;auto Supported=[&](float X,float Y,float Expected,float Tolerance){FHitResult H;++Samples;const bool Hit=W->LineTraceSingleByChannel(H,FVector(X,Y,Expected+45),FVector(X,Y,Expected-90),ECC_Pawn,GroundQ);const bool OK=Hit&&FMath::Abs(H.ImpactPoint.Z-Expected)<Tolerance&&H.ImpactNormal.Z>.70f;if(!OK)Note(FString::Printf(TEXT("Support failed at (%.1f,%.1f) expectedZ=%.1f hit=%d actual=%s normal=%s component=%s"),X,Y,Expected,int(Hit),*H.ImpactPoint.ToString(),*H.ImpactNormal.ToString(),*GetNameSafe(H.GetComponent())));return OK;};
  for(const auto& H:HouseWoods::Hollows()){float Z=HouseWoods::Height(H.Center.X,H.Center.Y);if(Z>-430||!Supported(H.Center.X,H.Center.Y,Z,8)){Finish(false,TEXT("Hollow lacks a recessed solid bottom"));return;}}
  for(float X=-7000;X<8000;X+=1800)for(float Y=5000;Y<18500;Y+=2100)if(!Supported(X,Y,HouseWoods::Height(X,Y),12)){Finish(false,TEXT("Hills have a ground gap at ")+FVector(X,Y,0).ToString());return;}
  for(const FVector2D P:HouseWoods::Trail())if(!Supported(P.X,P.Y,HouseWoods::Height(P.X,P.Y),25)){Finish(false,TEXT("Trail floor support is missing"));return;}
  for(float X:{-1000.f,-800.f,-630.f})for(float Y:{13400.f,13620.f})if(!Supported(X,Y,HouseWoods::HutFloor,3)){Finish(false,TEXT("Hut floor support is missing"));return;}
  FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(WoodsHutEntry),true,Pawn);const FVector A(HouseWoods::HutX,HouseWoods::HutY-345,HouseWoods::HutFloor+Half+2),B(HouseWoods::HutX,HouseWoods::HutY+100,HouseWoods::HutFloor+Half+2);
  if(W->SweepSingleByChannel(Hit,A,B,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half),Q)){Finish(false,TEXT("Hut entry capsule blocked by ")+GetNameSafe(Hit.GetComponent()));return;}
  Note(FString::Printf(TEXT("Actual scene: logs=%d branches=%d flowerPatches=%d rocks=%d leafPatches=%d treeHeightScale=%.2f..%.2f floorSamples=%d; three hollow bottoms solid; hut entry capsule clear"),Logs,Branches,Flowers,Rocks,Leaves,Minimum,Maximum,Samples));
  if(!Walk){Finish(true,TEXT("Native woodland scene/collision checks passed"));return;}
  const auto& Curve=HouseWoods::SmoothTrail();for(int I=0;I<Curve.Num();I+=5)S.Route.Add(Curve[I]);if(!S.Route.Last().Equals(Curve.Last(),1))S.Route.Add(Curve.Last());
  const auto P=S.Route[0];Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->SetActorLocation(FVector(P.X,P.Y,HouseWoods::Height(P.X,P.Y)+Half+2),false,nullptr,ETeleportType::TeleportPhysics);Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  S.Phase=1;S.Step=Now;const auto Next=S.Route[1];S.Goal=FVector(Next.X,Next.Y,0);return;
 }
 if(S.Phase==1)
 {
  const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Half);PC->SetControlRotation((S.Goal-Feet+FVector(0,0,HouseWoods::Height(S.Goal.X,S.Goal.Y))).Rotation());
  const float Expected=HouseWoods::Height(Feet.X,Feet.Y)+(Feet.Y>HouseWoods::HutY-425&&FMath::Abs(Feet.X-HouseWoods::HutX)<205?18.f:0.f);
  if(Now-S.Step>14||Feet.Z<Expected-45){FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(WoodsBlocker),false,Pawn);W->SweepSingleByChannel(H,Pawn->GetActorLocation(),Pawn->GetActorLocation()+(S.Goal-Feet).GetSafeNormal2D()*100,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half),Q);Finish(false,TEXT("Actual trail walk stalled at ")+Feet.ToString()+TEXT(" blocker=")+GetNameSafe(H.GetComponent()));return;}
  if(FVector::Dist2D(Feet,S.Goal)<40){Pawn->GetCharacterMovement()->StopMovementImmediately();Note(FString::Printf(TEXT("Walking leg %d reached %s"),S.Leg,*Feet.ToString()));++S.Leg;S.Step=Now;
   if(S.Leg>S.Route.Num()){FScreenshotRequest::RequestScreenshot(S.Dir/TEXT("HutEntered.png"),false,false);S.Phase=2;S.Step=Now;return;}
   if(S.Leg==S.Route.Num())S.Goal=FVector(HouseWoods::HutX,HouseWoods::HutY+90,0);else{const auto Next=S.Route[S.Leg];S.Goal=FVector(Next.X,Next.Y,0);}
  }
 }
 if(S.Phase==2&&Now-S.Step>1)Finish(true,TEXT("Player walked the full woodland trail, up the porch steps and through the hut doorway without a collision stall or ground drop. Single-player check."));
}
#else
void TickHouseWoodsReview(UWorld*){}
#endif
