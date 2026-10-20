// Opt-in verification of the installed photo kitchen. No normal-game changes.
#include "StoryCampaign.h"
#include "ExistingHouseDoor.h"
#include "HouseInteractiveProp.h"
#include "HouseFurnitureInteractionComponent.h"
#if !UE_BUILD_SHIPPING
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"
namespace
{
struct FKitchenCheck
{
    int32 Phase=0, Count=0, Hatches=0, ServingCarts=0;
    double Started=0, Changed=0;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;
    TWeakObjectPtr<AExistingHouseDoor> Door;
    TWeakObjectPtr<AHouseInteractiveProp> Chair;
    TWeakObjectPtr<AHouseInteractiveProp> Table;
    TWeakObjectPtr<AHouseInteractiveProp> Fridge, PrepRack;
    TSharedPtr<FJsonObject> Report=MakeShared<FJsonObject>();
    FString Directory;
};
void Finish(FKitchenCheck& S, bool Passed, const FString& Reason)
{
    S.Report->SetBoolField(TEXT("passed"),Passed);S.Report->SetStringField(TEXT("detail"),Reason);
    S.Report->SetNumberField(TEXT("furnitureCount"),S.Count);S.Report->SetNumberField(TEXT("ceilingPanels"),S.Hatches);
    S.Report->SetNumberField(TEXT("elapsedSeconds"),FPlatformTime::Seconds()-S.Started);
    FString Json;FJsonSerializer::Serialize(S.Report.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
    IFileManager::Get().MakeDirectory(*S.Directory,true);FFileHelper::SaveStringToFile(Json,*(S.Directory/TEXT("result.json")));
    UE_LOG(LogTemp,Display,TEXT("KITCHEN_CHECK_%s: %s"),Passed?TEXT("PASSED"):TEXT("FAILED"),*Reason);
    S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
bool DoorSweep(UWorld* World,AStoryFirstPersonCharacter* Pawn,AExistingHouseDoor* Door,FHitResult& Hit)
{
    const FVector Center(Door->DoorwayCenter.X,Door->DoorwayCenter.Y,0.f+Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+1.f);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(KitchenClosetGate),false,Pawn);
    return World->SweepSingleByChannel(Hit,Center+FVector(90,0,0),Center-FVector(45,0,0),FQuat::Identity,ECC_Pawn,
        FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),Q);
}
}
void TickHouseKitchenSmoke(UWorld* World)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("HouseKitchenSmoke")))return;
    static FKitchenCheck S;if(!World||S.Phase<0)return;
    const double Now=FPlatformTime::Seconds();
    if(S.Started==0){S.Started=Now;S.Changed=Now;FParse::Value(FCommandLine::Get(),TEXT("KitchenEvidence="),S.Directory);
        if(S.Directory.IsEmpty())S.Directory=FPaths::ProjectSavedDir()/TEXT("KitchenCheck");}
    if(Now-S.Started>60){Finish(S,false,TEXT("Kitchen check timed out"));return;}
    APlayerController* PC=World->GetFirstPlayerController();
    if(!PC||!PC->GetPawn())return;
    auto* Pawn=Cast<AStoryFirstPersonCharacter>(PC->GetPawn());if(!Pawn||!Pawn->FurnitureInteraction)return;
    if(S.Phase==0)
    {
        if(Now-S.Changed<3)return;
        PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);Pawn->GetCharacterMovement()->StopMovementImmediately();S.Pawn=Pawn;
        for(TActorIterator<AExistingHouseDoor> It(World);It;++It)if(It->DoorId==TEXT("ReferenceKitchen_Closet"))S.Door=*It;
        if(!S.Door.IsValid()||!S.Door->bLocked){Finish(S,false,TEXT("Closed kitchen closet missing or unlocked"));return;}
        for(TActorIterator<AActor> It(World);It;++It)
        {
            if(!It->ActorHasTag(TEXT("SSOKitchenRefit")))continue;
            if(It->ActorHasTag(TEXT("KitchenVisualOnly")))
            {
                auto* Visual=It->FindComponentByClass<UStaticMeshComponent>();
                if(Visual&&Visual->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)
                {Finish(S,false,TEXT("Visual floor or wall detail unexpectedly blocks the player"));return;}
            }
            if(It->GetName().Contains(TEXT("CeilingHatchHardware")))++S.Hatches;
            auto* Prop=Cast<AHouseInteractiveProp>(*It);if(!Prop)continue;++S.Count;
            if(Prop->bPhysicsActive||Prop->Body->IsSimulatingPhysics()||Prop->IsActorTickEnabled())
            {Finish(S,false,TEXT("New untouched furniture is simulating or ticking"));return;}
            for(UStaticMesh* Mesh:{Prop->IntactMesh.Get(),Prop->DamagedMesh.Get(),Prop->HeavilyDamagedMesh.Get(),Prop->BrokenMesh.Get()})
            {
                if(!Mesh||!Mesh->GetBodySetup()||Mesh->GetBodySetup()->AggGeom.ConvexElems.IsEmpty())
                {Finish(S,false,TEXT("Kitchen prop lacks a damage stage or fitted hull"));return;}
                for(int32 I=0;I<Mesh->GetStaticMaterials().Num();++I)if(!Mesh->GetMaterial(I))
                {Finish(S,false,TEXT("Kitchen mesh has an unassigned material"));return;}
            }
            if(Prop->PropId==TEXT("Kitchen_GreenSpindleChair_001"))S.Chair=Prop;
            if(Prop->PropId==TEXT("Kitchen_DiningTable_001"))S.Table=Prop;
            if(Prop->PropId.ToString().StartsWith(TEXT("Kitchen_ServingCounter_")))++S.ServingCarts;
            if(Prop->PropId==TEXT("Kitchen_TopFreezerFridge_001"))S.Fridge=Prop;
            if(Prop->PropId==TEXT("Kitchen_StainlessApplianceRack_001"))S.PrepRack=Prop;
        }
        // Tags, rather than editor names, survive runtime object duplication.
        S.Hatches=0;for(TActorIterator<AActor> It(World);It;++It)if(It->ActorHasTag(TEXT("LibraryCeilingPanel")))++S.Hatches;
        if(S.Count<100||!S.Chair.IsValid()||!S.Table.IsValid())
        {Finish(S,false,TEXT("Kitchen furniture inventory is incomplete"));return;}
        if(S.ServingCarts!=5||!S.Fridge.IsValid()||!S.PrepRack.IsValid()
            ||!FMath::IsNearlyEqual(S.Fridge->GetActorLocation().Y,1112.f,1.f)
            ||!FMath::IsNearlyEqual(S.PrepRack->GetActorLocation().Y,927.f,1.f))
        {Finish(S,false,TEXT("Two additional serving carts or corrected appliance positions are missing"));return;}
        const FVector ChairSize=S.Chair->IntactMesh->GetBounds().BoxExtent*2;
        if(FMath::Max(ChairSize.X,ChairSize.Y)>60.f||ChairSize.Z<88.f||ChairSize.Z>95.f)
        {Finish(S,false,TEXT("The installed chair still has stretched proportions"));return;}
        S.Report->SetNumberField(TEXT("stainlessServingCarts"),S.ServingCarts);
        S.Report->SetBoolField(TEXT("fridgeAndPrepTableCorrectOrder"),true);
        S.Report->SetNumberField(TEXT("chairMaximumHorizontalSizeCm"),FMath::Max(ChairSize.X,ChairSize.Y));
        S.Report->SetBoolField(TEXT("allPropsParkedWithDamageAndCollision"),true);
        FHitResult Hit;if(!DoorSweep(World,Pawn,S.Door.Get(),Hit)||Hit.GetActor()!=S.Door.Get())
        {
            auto* Door=S.Door.Get(); UBoxComponent* Panel=nullptr;TArray<UBoxComponent*> Boxes;Door->GetComponents(Boxes);for(auto* B:Boxes)if(B->GetName()==TEXT("MovingPanelCollision"))Panel=B; auto* Leaf=Door->FindComponentByClass<UStaticMeshComponent>();
            UE_LOG(LogTemp,Error,TEXT("KITCHEN_DOOR_DIAG hit=%s component=%s startPenetrating=%d impact=%s panel=%s ext=%s collision=%d mesh=%s"),
                *GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),Hit.bStartPenetrating,*Hit.ImpactPoint.ToString(),
                *Panel->GetComponentLocation().ToString(),*Panel->GetScaledBoxExtent().ToString(),int32(Panel->GetCollisionEnabled()),*GetNameSafe(Leaf->GetStaticMesh()));
            Finish(S,false,TEXT("Closed kitchen closet does not block the player capsule"));return;}
        Pawn->SetActorLocation(FVector(2200,105,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+1));
        if(S.Door->Interact(Pawn)){Finish(S,false,TEXT("Closed closet should remain locked"));return;}
        S.Report->SetBoolField(TEXT("closedClosetBlocksAndStaysClosed"),true);
        S.Phase=2;S.Changed=Now;return;
    }
    if(S.Phase==2)
    {
        if(Now-S.Changed<1)return;
        FHitResult Hit;if(!DoorSweep(World,Pawn,S.Door.Get(),Hit))
        {Finish(S,false,TEXT("Closed kitchen closet lost collision"));return;}
        auto* Chair=S.Chair.Get();FVector C,E;Chair->GetActorBounds(false,C,E);
        // Use a free floor section for the pickup check, with the actual authored chair.
        Chair->SetActorLocation(FVector(2360,350,0),false,nullptr,ETeleportType::TeleportPhysics);
        Chair->SetActorRotation(FRotator::ZeroRotator);
        Pawn->SetActorLocation(FVector(2360,200,0+Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+1),false,nullptr,ETeleportType::TeleportPhysics);
        PC->SetControlRotation((Chair->Body->Bounds.Origin-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        S.Phase=3;S.Changed=Now;return;
    }
    if(S.Phase==3)
    {
        if(Now-S.Changed<.3)return;
        if(Pawn->FurnitureInteraction->FocusedFurnitureForInteraction()!=S.Chair.Get())
        {Finish(S,false,TEXT("Actual kitchen chair could not be targeted for pickup"));return;}
        Pawn->FurnitureInteraction->ToggleGrab();S.Phase=4;S.Changed=Now;return;
    }
    if(S.Phase==4)
    {
        if(Now-S.Changed<1.5)return;
        if(!Pawn->FurnitureInteraction->bHoldingFurniture||Pawn->FurnitureInteraction->GetHeldBody()!=S.Chair->Body)
        {Finish(S,false,TEXT("Pickup did not hold the kitchen chair"));return;}
        FVector L,R,N;if(!Pawn->FurnitureInteraction->GetHandContact(false,L,N)||!Pawn->FurnitureInteraction->GetHandContact(true,R,N))
        {Finish(S,false,TEXT("Chair carry lacks two real hand contacts"));return;}
        S.Report->SetBoolField(TEXT("actualChairPickupAndTwoHandContacts"),true);
        Pawn->FurnitureInteraction->ReleaseGrab();
        auto* Table=S.Table.Get();Table->SetActorLocation(FVector(2350,800,0),false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->SetActorLocation(FVector(2480,500,0+Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+1),false,nullptr,ETeleportType::TeleportPhysics);
        S.Phase=5;S.Changed=Now;return;
    }
    if(S.Phase>=5&&S.Phase<=7)
    {
        if(Now-S.Changed<1.6)return;
        auto* Table=S.Table.Get();const float D=S.Phase==5?Table->BreakHealth*.35f:S.Phase==6?Table->BreakHealth*.35f:Table->BreakHealth*2;
        FDamageEvent Event;Table->TakeDamage(D,Event,PC,Pawn);
        if(Table->DamageStage!=S.Phase-4){Finish(S,false,TEXT("New table damage stage mismatch"));return;}
        ++S.Phase;S.Changed=Now;return;
    }
    if(S.Phase==8)
    {
        if(Now-S.Changed<4)return;
        const float Speed=S.Table->Body->GetPhysicsLinearVelocity().Size();
        if(Speed>20||!S.Table->bBroken){Finish(S,false,TEXT("Broken kitchen furniture did not settle"));return;}
        S.Report->SetBoolField(TEXT("allThreeTableDamageStagesAndSettling"),true);S.Report->SetNumberField(TEXT("settledBrokenSpeed"),Speed);
        Finish(S,true,TEXT("Installed kitchen inventory, fitted damage meshes, closed closet collision, actual chair pickup and staged table destruction verified"));
    }
}
#else
void TickHouseKitchenSmoke(UWorld*){}
#endif
