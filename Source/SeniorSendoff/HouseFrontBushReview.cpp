// Opt-in visual-upgrade acceptance route. No effect on normal play or saved maps.
#include "HouseCarryTiersReview.h"
#if !UE_BUILD_SHIPPING
#include "StoryCampaign.h"
#include "HouseExterior.h"
#include "HouseBlackHall.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"

void TickHouseFrontBushReview(AStoryFirstPersonCharacter* Pawn)
{
    if(!Pawn||!Pawn->HasAuthority()||!Pawn->IsLocallyControlled())return;
    UWorld* World=Pawn->GetWorld();
    if(!World||UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    auto* PC=Cast<APlayerController>(Pawn->GetController());if(!PC)return;
    struct FState {TWeakObjectPtr<UWorld> World;int Phase=0,Route=0,Point=1;double Start=0,Step=0;FString Log,Dir;};
    static FState State;const double Now=World->GetTimeSeconds();
    const float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    auto Note=[&](const FString& Message){State.Log+=Message+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("FRONT_BUSH_REVIEW %s"),*Message);};
    auto Finish=[&](bool Pass,const FString& Message)
    {
        Note((Pass?TEXT("PASS "):TEXT("FAIL "))+Message);
        FFileHelper::SaveStringToFile(State.Log,*(State.Dir/TEXT("result.txt")));
        State.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);
    };
    auto Floor=[&](const FVector& P,FHitResult& Hit)
    {
        FCollisionQueryParams Q(SCENE_QUERY_STAT(FrontBushGround),false,Pawn);
        return World->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,150),FVector(P.X,P.Y,-300),ECC_Pawn,Q)&&Hit.ImpactNormal.Z>.65f;
    };
    auto Place=[&](const FVector& P)
    {
        FHitResult Hit;if(!Floor(P,Hit))return false;
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->SetActorLocation(FVector(P.X,P.Y,Hit.ImpactPoint.Z+Half+2),false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);return true;
    };
    const TArray<TArray<FVector>> Routes={
        {{-36.65,-3020,0},{-36.65,-2430,0},{-36.65,-2010,0},{1050,-1900,0},{-1180,-1900,0},{-36.65,-2010,0},{-36.65,-3010,0}},
        {{-3190,-2290,0},{-3475,-2290,0},{-3475,-1800,0},{-3475,-1000,0}},
        {{3030,-2220,0},{3440,-2220,0},{3440,-1750,0},{3440,-1000,0}}
    };
    if(State.World.Get()!=World)
    {
        State=FState();State.World=World;State.Start=Now;State.Dir=FPaths::ProjectSavedDir()/TEXT("FrontBushReview");
        IFileManager::Get().MakeDirectory(*State.Dir,true);
        if(auto* Settings=UGameUserSettings::GetGameUserSettings()){Settings->SetOverallScalabilityLevel(2);Settings->ApplyNonResolutionSettings();}
    }
    if(State.Phase<0)return;
    if(Now-State.Start>140){Finish(false,TEXT("Front planting-bed review timed out"));return;}
    if(State.Phase==0)
    {
        if(Now-State.Start<7)return;
        int Plants=0,Batches=0,LegacyPlants=0;
        for(TActorIterator<AHouseExteriorDetail> It(World);It;++It)
        {
            TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components(*It);
            for(auto* Component:Components)
            {
                const FString Name=Component->GetName();
                if(Name.StartsWith(TEXT("ExteriorBushDetailed")))LegacyPlants+=Component->GetInstanceCount();
                if(!Name.StartsWith(TEXT("ExteriorBushFrontDetailed")))continue;
                ++Batches;Plants+=Component->GetInstanceCount();
                UStaticMesh* Mesh=Component->GetStaticMesh();auto* Body=Mesh?Mesh->GetBodySetup():nullptr;
                if(!Body||Body->AggGeom.BoxElems.Num()!=1){Finish(false,TEXT("New bush has lost the original simple blocker"));return;}
                const auto& Box=Body->AggGeom.BoxElems[0];
                if(!FMath::IsNearlyEqual(Box.X,140.f,.05f)||!FMath::IsNearlyEqual(Box.Y,140.f,.05f)||!FMath::IsNearlyEqual(Box.Z,100.f,.05f))
                {Finish(false,TEXT("Visual upgrade changed shrub collision dimensions"));return;}
                for(int Index=0;Index<Component->GetInstanceCount();++Index)
                {
                    FTransform Transform;Component->GetInstanceTransform(Index,Transform,true);
                    const FVector P=Transform.GetLocation();
                    if(P.Y>=-2100||P.X<=-3400||P.X>=3400){Finish(false,TEXT("New mesh leaked outside the front beds"));return;}
                }
                int LeafMaterials=0;
                for(int Index=0;Index<Component->GetNumMaterials();++Index)
                {
                    const auto* Material=Cast<UMaterialInstanceDynamic>(Component->GetMaterial(Index));
                    if(!Material||!Material->Parent||!Material->Parent->GetPathName().Contains(TEXT("M_FrontShrubFoliage")))continue;
                    ++LeafMaterials;
                    float Wind=0;
                    if(!Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("WindStrength")),Wind)||Wind<=0||Wind>1)
                    {Finish(false,TEXT("Missing gentle leaf-tip wind"));return;}
                }
                if(LeafMaterials<5){Finish(false,TEXT("Photographic leaf atlas is not assigned to the new meshes"));return;}
            }
        }
        if(Plants!=31||Batches!=3){Finish(false,FString::Printf(TEXT("Expected 31 front plants in three batches; got %d/%d"),Plants,Batches));return;}
        Note(FString::Printf(TEXT("31 original front placements; 3 instanced meshes; original 140x140x100 blockers; leaf atlas/wind assigned. Side bushes=%d"),LegacyPlants));
        if(!Place(Routes[0][0])){Finish(false,TEXT("Cannot stand on the front walkway"));return;}
        State.Phase=1;State.Step=Now;return;
    }
    const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Half);
    FHitResult Ground;
    if(!Floor(Feet,Ground)||Feet.Z<Ground.ImpactPoint.Z-10||Feet.Z>Ground.ImpactPoint.Z+35)
    {Finish(false,FString::Printf(TEXT("Lost supported footing at %s"),*Feet.ToString()));return;}
    FVector To=Routes[State.Route][State.Point]-Feet;To.Z=0;
    if(To.Size()<18)
    {
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Note(FString::Printf(TEXT("Normal movement completed route %d segment %d; feet=%s"),State.Route,State.Point,*Feet.ToString()));
        if(++State.Point>=Routes[State.Route].Num())
        {
            if(++State.Route>=Routes.Num())
            {Finish(true,TEXT("Walkway to steps, porch across both hedge beds, return to lawn, and both side paths walked without clipping or blockage. Single-player development test; multiplayer not tested."));return;}
            if(!Place(Routes[State.Route][0])){Finish(false,TEXT("No support at side-path start"));return;}
            State.Point=1;
        }
        State.Step=Now;return;
    }
    if(Now-State.Step>18)
    {Finish(false,FString::Printf(TEXT("Normal walking blocked on route %d segment %d at %s"),State.Route,State.Point,*Feet.ToString()));return;}
    Pawn->AddMovementInput(To.GetSafeNormal(),1);PC->SetControlRotation(To.Rotation());
}
#else
void TickHouseFrontBushReview(AStoryFirstPersonCharacter*){}
#endif
