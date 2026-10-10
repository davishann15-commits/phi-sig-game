// Opt-in verification of the installed photo library. No normal-game changes.
#include "StoryCampaign.h"
#include "ExistingHouseDoor.h"
#include "HouseInteractiveProp.h"
#include "HouseFurnitureInteractionComponent.h"
#if !UE_BUILD_SHIPPING
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
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
struct FLibraryCheck
{
    int32 Phase=0, Count=0, Hatches=0, SmallItems=0;
    double Started=0, Changed=0;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;
    TWeakObjectPtr<AExistingHouseDoor> Door;
    TWeakObjectPtr<AHouseInteractiveProp> Chair;
    TWeakObjectPtr<AHouseInteractiveProp> Table;
    TWeakObjectPtr<AHouseInteractiveProp> Book, Paper, Shelf, Witness, DamageShelf;
    FVector WitnessLocation=FVector::ZeroVector;
    TArray<TWeakObjectPtr<AHouseInteractiveProp>> ShelfContents;
    TSharedPtr<FJsonObject> Report=MakeShared<FJsonObject>();
    FString Directory;
};
void Finish(FLibraryCheck& S, bool Passed, const FString& Reason)
{
    S.Report->SetBoolField(TEXT("passed"),Passed);S.Report->SetStringField(TEXT("detail"),Reason);
    S.Report->SetNumberField(TEXT("furnitureCount"),S.Count);S.Report->SetNumberField(TEXT("ceilingPanels"),S.Hatches);
    S.Report->SetNumberField(TEXT("elapsedSeconds"),FPlatformTime::Seconds()-S.Started);
    FString Json;FJsonSerializer::Serialize(S.Report.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
    IFileManager::Get().MakeDirectory(*S.Directory,true);FFileHelper::SaveStringToFile(Json,*(S.Directory/TEXT("result.json")));
    UE_LOG(LogTemp,Display,TEXT("LIBRARY_CHECK_%s: %s"),Passed?TEXT("PASSED"):TEXT("FAILED"),*Reason);
    S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
bool DoorSweep(UWorld* World,AStoryFirstPersonCharacter* Pawn,AExistingHouseDoor* Door,FHitResult& Hit)
{
    const FVector Center(Door->DoorwayCenter.X,Door->DoorwayCenter.Y,-329.06f+Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+1.f);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(LibraryDoorGate),false,Pawn);
    return World->SweepSingleByChannel(Hit,Center-FVector(0,60,0),Center+FVector(0,145,0),FQuat::Identity,ECC_Pawn,
        FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),Q);
}
}
void TickHouseLibrarySmoke(UWorld* World)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("HouseLibrarySmoke")))return;
    static FLibraryCheck S;if(!World||S.Phase<0)return;
    const double Now=FPlatformTime::Seconds();
    if(S.Started==0){S.Started=Now;S.Changed=Now;FParse::Value(FCommandLine::Get(),TEXT("LibraryEvidence="),S.Directory);
        if(S.Directory.IsEmpty())S.Directory=FPaths::ProjectSavedDir()/TEXT("LibraryCheck");}
    if(Now-S.Started>60){Finish(S,false,TEXT("Library check timed out"));return;}
    APlayerController* PC=World->GetFirstPlayerController();
    if(!PC||!PC->GetPawn())return;
    auto* Pawn=Cast<AStoryFirstPersonCharacter>(PC->GetPawn());if(!Pawn||!Pawn->FurnitureInteraction)return;
    if(S.Phase==0)
    {
        if(Now-S.Changed<3)return;
        PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);Pawn->GetCharacterMovement()->StopMovementImmediately();S.Pawn=Pawn;
        for(TActorIterator<AExistingHouseDoor> It(World);It;++It)if(It->DoorId==TEXT("BasementLibrary_Mechanical"))S.Door=*It;
        if(!S.Door.IsValid()||S.Door->bLocked){Finish(S,false,TEXT("Mechanical door missing or locked"));return;}
        for(TActorIterator<AActor> It(World);It;++It)
        {
            if(!It->ActorHasTag(TEXT("SSOLibraryRebuild")))continue;
            if(It->GetName().Contains(TEXT("CeilingHatchHardware")))++S.Hatches;
            auto* Prop=Cast<AHouseInteractiveProp>(*It);if(!Prop)continue;++S.Count;
            if(Prop->ActorHasTag(TEXT("SSOLibrarySmallItem")))
            {
                ++S.SmallItems;
                if(Prop->GetAttachParentActor()||Prop->Body->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Ignore)
                {Finish(S,false,TEXT("Loose library item is attached or catches player capsules"));return;}
            }
            if(Prop->PropId.ToString().StartsWith(TEXT("Library_BookShelfContents"))||Prop->PropId.ToString().StartsWith(TEXT("Library_StackedBooks"))
                ||Prop->PropId.ToString().StartsWith(TEXT("Library_PaperScatter"))||Prop->PropId.ToString().StartsWith(TEXT("Library_LoosePaperStack")))
            {Finish(S,false,TEXT("Old fused book or paper group remains"));return;}
            if(Prop->bPhysicsActive||Prop->Body->IsSimulatingPhysics()||Prop->IsActorTickEnabled())
            {Finish(S,false,TEXT("New untouched furniture is simulating or ticking"));return;}
            for(UStaticMesh* Mesh:{Prop->IntactMesh.Get(),Prop->DamagedMesh.Get(),Prop->HeavilyDamagedMesh.Get(),Prop->BrokenMesh.Get()})
            {
                if(!Mesh||!Mesh->GetBodySetup()||Mesh->GetBodySetup()->AggGeom.ConvexElems.IsEmpty())
                {Finish(S,false,TEXT("Library prop lacks a damage stage or fitted hull"));return;}
                for(int32 I=0;I<Mesh->GetStaticMaterials().Num();++I)if(!Mesh->GetMaterial(I))
                {Finish(S,false,TEXT("Library mesh has an unassigned material"));return;}
            }
            if(Prop->PropId==TEXT("Library_MeshOfficeChair_005"))S.Chair=Prop;
            if(Prop->PropId==TEXT("Library_CoffeeChest_001"))S.Table=Prop;
            if(Prop->PropId.ToString().StartsWith(TEXT("Library_Loose_BookShelfContents")))
            {if(!S.Book.IsValid())S.Book=Prop;else if(!S.Witness.IsValid())S.Witness=Prop;}
            if(Prop->PropId.ToString().StartsWith(TEXT("Library_Loose_PaperScatter"))&&!S.Paper.IsValid())S.Paper=Prop;
            if(Prop->PropId.ToString().StartsWith(TEXT("Library_SixShelfBookcase"))&&!S.Shelf.IsValid())S.Shelf=Prop;
            if(Prop->PropId==TEXT("Library_SixShelfBookcase_002"))S.DamageShelf=Prop;
        }
        // Tags, rather than editor names, survive runtime object duplication.
        S.Hatches=0;for(TActorIterator<AActor> It(World);It;++It)if(It->ActorHasTag(TEXT("LibraryCeilingPanel")))++S.Hatches;
        if(S.Count<120||S.Hatches!=3||!S.Chair.IsValid()||!S.Table.IsValid())
        {Finish(S,false,TEXT("Library furniture inventory is incomplete"));return;}
        S.Report->SetBoolField(TEXT("allPropsParkedWithDamageAndCollision"),true);
        if(S.SmallItems<400||!S.Book.IsValid()||!S.Paper.IsValid()||!S.Shelf.IsValid()||!S.Witness.IsValid())
        {Finish(S,false,TEXT("Independent library clutter inventory incomplete"));return;}
        S.Report->SetNumberField(TEXT("independentSmallItems"),S.SmallItems);
        int32 Fireplaces=0;
        for(TActorIterator<AActor> It(World);It;++It)if(It->ActorHasTag(TEXT("GreatRoomSolidFireplace")))
        {
            ++Fireplaces;
            const FVector Origin=It->GetActorLocation();
            const FVector Out=It->GetActorRotation().RotateVector(FVector(0,1,0));
            const FVector Across=It->GetActorRotation().RotateVector(FVector(1,0,0));
            for(float Offset:{-135.f,0.f,135.f})
            {
                FVector Start=Origin+Out*185.f+Across*Offset;
                Start.Z=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+40.f;
                FHitResult Block;FCollisionQueryParams Q(SCENE_QUERY_STAT(FireplacePlayerSweep),false,Pawn);
                for(TActorIterator<AHouseInteractiveProp> Other(World);Other;++Other)Q.AddIgnoredActor(*Other);
                if(!World->SweepSingleByChannel(Block,Start,Start-Out*215.f,FQuat::Identity,ECC_Pawn,
                    FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),Q)
                    ||Block.GetActor()!=*It)
                {Finish(S,false,FString::Printf(TEXT("Fireplace does not stop a full player capsule: %s offset %.1f hit %s"),*It->GetName(),Offset,*GetNameSafe(Block.GetActor())));return;}
            }
        }
        if(Fireplaces!=2){Finish(S,false,TEXT("Both solid great room fireplaces are required"));return;}
        S.Report->SetNumberField(TEXT("fireplacePlayerCapsuleSweepsBlocked"),6);
        S.WitnessLocation=S.Witness->GetActorLocation();
        FHitResult Hit;if(!DoorSweep(World,Pawn,S.Door.Get(),Hit)||Hit.GetActor()!=S.Door.Get())
        {Finish(S,false,TEXT("Closed service door does not block the player capsule"));return;}
        Pawn->SetActorLocation(FVector(-2701,710,-329.06+Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+1));
        if(!S.Door->Interact(Pawn)){Finish(S,false,TEXT("Door input did not start the opening"));return;}
        S.Phase=1;S.Changed=Now;return;
    }
    if(S.Phase==1)
    {
        if(Now-S.Changed<1.3)return;
        FHitResult Hit;if(DoorSweep(World,Pawn,S.Door.Get(),Hit))
        {
            auto* Block=Cast<UStaticMeshComponent>(Hit.GetComponent());
            Finish(S,false,FString::Printf(TEXT("Open service passage blocked by %s mesh=%s impact=%s normal=%s"),
                Hit.GetActor()?*Hit.GetActor()->GetName():TEXT("unknown"),
                Block&&Block->GetStaticMesh()?*Block->GetStaticMesh()->GetPathName():TEXT("none"),
                *Hit.ImpactPoint.ToCompactString(),*Hit.ImpactNormal.ToCompactString()));return;
        }
        S.Report->SetBoolField(TEXT("closedDoorBlocksAndOpenDoorClearsFullPlayer"),true);
        Pawn->SetActorLocation(FVector(-2701,1010,-329.06+Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+1),false,nullptr,ETeleportType::TeleportPhysics);
        if(!S.Door->Interact(Pawn)){Finish(S,false,TEXT("Service door failed to close"));return;}
        S.Phase=2;S.Changed=Now;return;
    }
    if(S.Phase==2)
    {
        if(Now-S.Changed<1)return;
        FHitResult Hit;if(!DoorSweep(World,Pawn,S.Door.Get(),Hit))
        {Finish(S,false,TEXT("Closed service door lost collision"));return;}
        auto* Chair=S.Chair.Get();FVector C,E;Chair->GetActorBounds(false,C,E);
        // Use a free floor section for the pickup check, with the actual authored chair.
        Chair->SetActorLocation(FVector(-2540,-770,-329.06),false,nullptr,ETeleportType::TeleportPhysics);
        Chair->SetActorRotation(FRotator::ZeroRotator);
        const FVector ChairSize=Chair->Body->Bounds.BoxExtent*2;
        if(ChairSize.X>75||ChairSize.Y>75)
        {Finish(S,false,TEXT("Rolling chair is still oversized"));return;}
        S.Report->SetNumberField(TEXT("rollingChairWidthCm"),ChairSize.X);
        S.Report->SetNumberField(TEXT("rollingChairDepthCm"),ChairSize.Y);
        Pawn->SetActorLocation(FVector(-2540,-920,-329.06+Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+1),false,nullptr,ETeleportType::TeleportPhysics);
        PC->SetControlRotation((Chair->Body->Bounds.Origin-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        S.Phase=3;S.Changed=Now;return;
    }
    if(S.Phase==3)
    {
        if(Now-S.Changed<.3)return;
        if(Pawn->FurnitureInteraction->FocusedFurnitureForInteraction()!=S.Chair.Get())
        {Finish(S,false,TEXT("Actual library chair could not be targeted for pickup"));return;}
        Pawn->FurnitureInteraction->ToggleGrab();S.Phase=4;S.Changed=Now;return;
    }
    if(S.Phase==4)
    {
        if(Now-S.Changed<1.5)return;
        if(!Pawn->FurnitureInteraction->bHoldingFurniture||Pawn->FurnitureInteraction->GetHeldBody()!=S.Chair->Body)
        {Finish(S,false,TEXT("Pickup did not hold the library chair"));return;}
        FVector L,R,N;if(!Pawn->FurnitureInteraction->GetHandContact(false,L,N)||!Pawn->FurnitureInteraction->GetHandContact(true,R,N))
        {Finish(S,false,TEXT("Chair carry lacks two real hand contacts"));return;}
        S.Report->SetBoolField(TEXT("actualChairPickupAndTwoHandContacts"),true);
        Pawn->FurnitureInteraction->ReleaseGrab();
        auto* Table=S.Table.Get();Table->SetActorLocation(FVector(-2760,-830,-329.06),false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->SetActorLocation(FVector(-2500,-1000,-329.06+Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+1),false,nullptr,ETeleportType::TeleportPhysics);
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
        if(Speed>20||!S.Table->bBroken){Finish(S,false,TEXT("Broken library furniture did not settle"));return;}
        S.Report->SetBoolField(TEXT("allThreeTableDamageStagesAndSettling"),true);S.Report->SetNumberField(TEXT("settledBrokenSpeed"),Speed);
        // Exercise a single book through the same focus and E path as the player.
        S.Chair->SetActorLocation(FVector(-2540,-650,-329.06),false,nullptr,ETeleportType::TeleportPhysics);
        S.Book->SetActorLocation(FVector(-2540,-800,-250),false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->SetActorLocation(FVector(-2540,-920,-329.06+Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+1),false,nullptr,ETeleportType::TeleportPhysics);
        PC->SetControlRotation((S.Book->Body->Bounds.Origin-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        S.Phase=9;S.Changed=Now;return;
    }
    if(S.Phase==9||S.Phase==12)
    {
        if(Now-S.Changed<.3)return;
        auto* Item=S.Phase==9?S.Book.Get():S.Paper.Get();
        AActor* Focused=Pawn->FurnitureInteraction->FocusedFurnitureForInteraction();
        if(Focused!=Item)
        {
            FVector View;FRotator Aim;PC->GetPlayerViewPoint(View,Aim);FHitResult Hit;
            FCollisionQueryParams Q(SCENE_QUERY_STAT(LibrarySmallFocusDiagnostic),false,Pawn);
            World->LineTraceSingleByChannel(Hit,View,View+Aim.Vector()*260,ECC_Visibility,Q);
            Finish(S,false,FString::Printf(TEXT("Individual item focus failed phase=%d item=%s focused=%s hit=%s view=%s aim=%s itemCenter=%s movable=%d"),
                S.Phase,*Item->PropId.ToString(),*GetNameSafe(Focused),*GetNameSafe(Hit.GetActor()),*View.ToCompactString(),*Aim.ToCompactString(),*Item->Body->Bounds.Origin.ToCompactString(),Item->CanBeMoved()));return;
        }
        Pawn->FurnitureInteraction->ToggleGrab();++S.Phase;S.Changed=Now;return;
    }
    if(S.Phase==10||S.Phase==13)
    {
        if(Now-S.Changed<1)return;
        auto* Item=S.Phase==10?S.Book.Get():S.Paper.Get();FVector Contact,Normal;
        if(!Pawn->FurnitureInteraction->bHoldingFurniture||Pawn->FurnitureInteraction->GetHeldBody()!=Item->Body
            ||UHouseFurnitureInteractionComponent::ClassifyFurniture(Item->Body,Item)!=ESSOCarrySize::Small
            ||!Pawn->FurnitureInteraction->GetHandContact(true,Contact,Normal))
        {Finish(S,false,TEXT("Loose item did not use small-item pickup and a real hand contact"));return;}
        if(!S.Witness->GetActorLocation().Equals(S.WitnessLocation,.1f)||S.Witness->bPhysicsActive)
        {Finish(S,false,TEXT("Picking up one book moved its untouched neighbour"));return;}
        S.Report->SetBoolField(S.Phase==10?TEXT("singleBookPickupIndependentOfNeighbour"):TEXT("singlePaperPickupWithHandContact"),true);
        PC->SetControlRotation(FRotator(12,180,0));Pawn->FurnitureInteraction->ThrowHeldFurniture();++S.Phase;S.Changed=Now;return;
    }
    if(S.Phase==11)
    {
        if(Now-S.Changed<.9)return;
        if(Pawn->FurnitureInteraction->bHoldingFurniture||!S.Book->Body->IsSimulatingPhysics())
        {Finish(S,false,TEXT("Individual book throw did not release"));return;}
        S.Paper->SetActorLocation(FVector(-2540,-800,-250),false,nullptr,ETeleportType::TeleportPhysics);
        PC->SetControlRotation((S.Paper->Body->Bounds.Origin-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        S.Phase=12;S.Changed=Now;return;
    }
    if(S.Phase==14)
    {
        if(Now-S.Changed<4)return;
        if(Pawn->FurnitureInteraction->bHoldingFurniture||!S.Paper->Body->IsSimulatingPhysics())
        {Finish(S,false,TEXT("Individual paper throw did not release"));return;}
        const float BookSpeed=S.Book->Body->GetPhysicsLinearVelocity().Size(),PaperSpeed=S.Paper->Body->GetPhysicsLinearVelocity().Size();
        if(BookSpeed>25||PaperSpeed>25||S.Book->Body->Bounds.Origin.Z<-350||S.Paper->Body->Bounds.Origin.Z<-350)
        {Finish(S,false,FString::Printf(TEXT("Thrown book/paper did not settle on collision: %.1f %.1f"),BookSpeed,PaperSpeed));return;}
        S.Report->SetBoolField(TEXT("individualBookAndPaperThrownAndSettled"),true);
        const FBox ShelfBox=S.Shelf->Body->Bounds.GetBox();
        for(TActorIterator<AHouseInteractiveProp> It(World);It;++It)
        {
            if(!It->PropId.ToString().StartsWith(TEXT("Library_Loose_BookShelfContents"))||It->bPhysicsActive)continue;
            if(ShelfBox.ExpandBy(FVector(2,2,15)).IsInside(It->Body->Bounds.Origin))S.ShelfContents.Add(*It);
        }
        if(S.ShelfContents.Num()<25){Finish(S,false,TEXT("No filled shelf available for support check"));return;}
        S.Shelf->WakeForInteraction(Pawn);
        int32 Woke=0;for(const auto& Item:S.ShelfContents)if(Item.IsValid()&&Item->bPhysicsActive)++Woke;
        if(Woke!=S.ShelfContents.Num())
        {Finish(S,false,FString::Printf(TEXT("Moving shelf left books frozen: %d of %d woke"),Woke,S.ShelfContents.Num()));return;}
        S.Report->SetNumberField(TEXT("shelfBooksWokenTogether"),Woke);
        S.Shelf->SetActorLocation(S.Shelf->GetActorLocation()+FVector(120,0,0),false,nullptr,ETeleportType::TeleportPhysics);
        S.Phase=15;S.Changed=Now;return;
    }
    if(S.Phase==15)
    {
        if(Now-S.Changed<5)return;
        int32 Fell=0;for(const auto& Item:S.ShelfContents)if(Item.IsValid()&&Item->Body->Bounds.Origin.Z<-295)++Fell;
        if(Fell<S.ShelfContents.Num()*.75f)
        {Finish(S,false,TEXT("Individual shelf books stayed floating after support moved"));return;}
        S.Report->SetNumberField(TEXT("shelfBooksFellToFloor"),Fell);
        if(!S.DamageShelf.IsValid()){Finish(S,false,TEXT("Shelf for staged break check missing"));return;}
        S.ShelfContents.Reset();
        const FName ContentTag(*FString::Printf(TEXT("ShelfContents_%s"),*S.DamageShelf->PropId.ToString()));
        for(TActorIterator<AHouseInteractiveProp> It(World);It;++It)if(It->ActorHasTag(ContentTag))S.ShelfContents.Add(*It);
        if(S.ShelfContents.Num()<40){Finish(S,false,TEXT("Filled shelf contents association incomplete"));return;}
        Pawn->SetActorLocation(FVector(-2700,-690.3,-329.06+Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+1),false,nullptr,ETeleportType::TeleportPhysics);
        PC->SetControlRotation((S.DamageShelf->Body->Bounds.Origin-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        S.Phase=16;S.Changed=Now;return;
    }
    if(S.Phase>=16&&S.Phase<=18)
    {
        if(Now-S.Changed<.7)return;
        if(S.Phase==18)S.DamageShelf->SetActorRotation(FRotator(0,S.DamageShelf->GetActorRotation().Yaw,55),ETeleportType::TeleportPhysics);
        FDamageEvent Event;S.DamageShelf->TakeDamage(S.Phase==18?500.f:S.DamageShelf->BreakHealth*.35f,Event,PC,Pawn);
        if(S.DamageShelf->DamageStage!=S.Phase-15)
        {Finish(S,false,TEXT("Shelf damage stage did not advance"));return;}
        int32 Awake=0;for(const auto& Item:S.ShelfContents)if(Item.IsValid()&&Item->bPhysicsActive)++Awake;
        if(Awake!=S.ShelfContents.Num())
        {Finish(S,false,FString::Printf(TEXT("Shelf break left contents parked: %d/%d"),Awake,S.ShelfContents.Num()));return;}
        S.Report->SetNumberField(TEXT("damagedShelfContentsWoken"),Awake);
        IFileManager::Get().MakeDirectory(*S.Directory,true);
        FScreenshotRequest::RequestScreenshot(S.Directory/FString::Printf(TEXT("ShelfStage%d.png"),S.Phase-15),false,false);
        ++S.Phase;S.Changed=Now;return;
    }
    if(S.Phase==19)
    {
        if(Now-S.Changed<6)return;
        FBox Wreck(ForceInit);
        auto* Body=S.DamageShelf->Body.Get();
        const auto* MeshData=Body->GetStaticMesh()->GetRenderData();
        if(!MeshData||MeshData->LODResources.IsEmpty()){Finish(S,false,TEXT("Shelf wreck render geometry missing"));return;}
        const auto& Vertices=MeshData->LODResources[0].VertexBuffers.PositionVertexBuffer;
        for(uint32 I=0;I<Vertices.GetNumVertices();++I)
            Wreck+=Body->GetComponentTransform().TransformPosition(FVector(Vertices.VertexPosition(I)));
        float CollisionBottom=MAX_flt;
        for(const FKConvexElem& Hull:Body->GetStaticMesh()->GetBodySetup()->AggGeom.ConvexElems)
            for(const FVector& V:Hull.VertexData)
                CollisionBottom=FMath::Min(CollisionBottom,float(Body->GetComponentTransform().TransformPosition(Hull.GetTransform().TransformPosition(V)).Z));
        S.Report->SetNumberField(TEXT("shelfWreckCollisionBottomCm"),CollisionBottom);
        if(!S.DamageShelf->bBroken||S.DamageShelf->IsHidden()||!S.DamageShelf->Body->IsVisible()
            ||Wreck.GetSize().Z<25.f||Wreck.Min.Z<-332.f||Wreck.Max.Z<-295.f||S.DamageShelf->BreakEvents!=1)
        {Finish(S,false,FString::Printf(TEXT("Shelf wreck disappeared or clipped under floor: min %.1f max %.1f events %d"),Wreck.Min.Z,Wreck.Max.Z,S.DamageShelf->BreakEvents));return;}
        int32 Fell=0;for(const auto& Item:S.ShelfContents)if(Item.IsValid()&&Item->Body->Bounds.Origin.Z<-225.f)++Fell;
        if(Fell<S.ShelfContents.Num()*.90f)
        {Finish(S,false,FString::Printf(TEXT("Broken shelf left contents floating: %d/%d fell"),Fell,S.ShelfContents.Num()));return;}
        S.Report->SetNumberField(TEXT("damagedShelfContentsFell"),Fell);
        S.Report->SetNumberField(TEXT("shelfWreckVisibleHeightCm"),Wreck.GetSize().Z);
        S.Report->SetNumberField(TEXT("shelfWreckBottomCm"),Wreck.Min.Z);
        S.Report->SetNumberField(TEXT("shelfWreckSpeed"),S.DamageShelf->Body->GetPhysicsLinearVelocity().Size());
        Pawn->GetCharacterMovement()->DisableMovement();Pawn->SetActorEnableCollision(false);
        Pawn->SetActorLocation(FVector(-2555,-600,-329.06+Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+1),false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->FirstPersonCamera->SetFieldOfView(85.f);
        PC->SetControlRotation((S.DamageShelf->Body->Bounds.Origin-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        S.Phase=20;S.Changed=Now;return;
    }
    if(S.Phase==20)
    {
        if(Now-S.Changed<1)return;
        FScreenshotRequest::RequestScreenshot(S.Directory/TEXT("ShelfSettled.png"),false,false);
        S.Phase=21;S.Changed=Now;return;
    }
    if(S.Phase==21)
    {
        if(Now-S.Changed<1)return;
        if(IFileManager::Get().FileSize(*(S.Directory/TEXT("ShelfSettled.png")))<=0){Finish(S,false,TEXT("Shelf damage screenshot missing"));return;}
        Finish(S,true,TEXT("Three shelf damage stages leave visible floor wreckage and release all books; both great room fireplaces block player capsules above the stepable hearth; loose item and chair interactions still pass"));
    }
}
#else
void TickHouseLibrarySmoke(UWorld*){}
#endif
