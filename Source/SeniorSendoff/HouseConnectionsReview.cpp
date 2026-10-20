#include "HouseCarryTiersReview.h"
#if !UE_BUILD_SHIPPING
#include "StoryCampaign.h"
#include "StoryMovementComponent.h"
#include "HouseReactiveFixture.h"
#include "ExistingHouseDoor.h"
#include "HouseInteractiveProp.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void TickHouseConnectionsReview(AStoryFirstPersonCharacter* Pawn)
{
    if(!Pawn || !Pawn->HasAuthority() || !Pawn->IsLocallyControlled())return;
    UWorld* W=Pawn->GetWorld();auto* PC=Cast<APlayerController>(Pawn->GetController());if(!PC)return;
    struct FState {TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ACameraActor> Cam;int Stage=0,View=0;double Start=0,Step=0;bool Jumped=false;FString Dir,Log;};
    static FState S;const double Now=W->GetTimeSeconds();
    if(S.World.Get()!=W){S=FState();S.World=W;S.Start=S.Step=Now;S.Dir=FPaths::Combine(FPaths::ProjectDir(),TEXT("SourceArt/HouseExteriorConnections/evidence/native"));IFileManager::Get().MakeDirectory(*S.Dir,true);}
    auto Note=[&](const FString& T){S.Log+=T+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("HOUSE_CONNECTIONS %s"),*T);};
    auto Finish=[&](bool OK){Note(OK?TEXT("PASS"):TEXT("FAIL"));FFileHelper::SaveStringToFile(S.Log,*FPaths::Combine(S.Dir,TEXT("result.txt")));S.Stage=-1;FPlatformMisc::RequestExitWithStatus(false,OK?0:1);};
    if(S.Stage<0)return;if(Now-S.Start>120){Finish(false);return;}
    struct FView {const TCHAR* Name;FVector Pos,Target;};
    const FView Views[]={
        {TEXT("front-door-jamb.png"),FVector(140,-1180,172),FVector(-90,-960,140)},
        {TEXT("porch-connected.png"),FVector(480,1100,173),FVector(290,926,138)},
        {TEXT("side-window-intact.png"),FVector(3480,-935,164),FVector(3235,-1061,108)},
        {TEXT("upper-window-connection.png"),FVector(-3590,850,566),FVector(-3285,1135,489)},
        {TEXT("front-connected-facade.png"),FVector(1200,-2840,690),FVector(-200,-1050,290)}
        ,{TEXT("front-right-corner.png"),FVector(1130,-1590,250),FVector(1450,-980,240)}
        ,{TEXT("left-front-restored.png"),FVector(-5600,-4300,300),FVector(-2800,-1800,300)}
        ,{TEXT("left-rear-restored.png"),FVector(-5500,3500,300),FVector(-3000,500,300)}
        ,{TEXT("right-end-restored.png"),FVector(6200,-2800,350),FVector(3340,-800,250)}
        ,{TEXT("court-facade-restored.png"),FVector(800,3400,-100),FVector(0,1000,260)}
        ,{TEXT("house-footprint-restored.png"),FVector(7000,-8000,3200),FVector(0,0,100)}
    };
    if(S.Stage==0)
    {
        if(Now-S.Start<9)return;
        if(!S.Cam.IsValid())S.Cam=W->SpawnActor<ACameraActor>();PC->SetViewTarget(S.Cam.Get());
        if(Now-S.Step<1.8)return;
        if(S.View<UE_ARRAY_COUNT(Views))
        {
            const auto& V=Views[S.View++];S.Cam->SetActorLocationAndRotation(V.Pos,(V.Target-V.Pos).Rotation());S.Cam->GetCameraComponent()->SetFieldOfView(77);
            if(S.View==5 || S.View==6)
            {
                const FRotationMatrix Basis(S.Cam->GetActorRotation());
                for(const FVector2D Pixel:{FVector2D(1040,340),FVector2D(1110,355),FVector2D(650,420)})
                {
                    const FVector Dir=(Basis.GetScaledAxis(EAxis::X)+Basis.GetScaledAxis(EAxis::Y)*((Pixel.X/640.f-1.f)*.795f)+Basis.GetScaledAxis(EAxis::Z)*((1.f-Pixel.Y/360.f)*.795f/1.77778f)).GetSafeNormal();
                    FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(ExteriorFinishAudit),true,Pawn);
                    W->LineTraceSingleByChannel(H,V.Pos,V.Pos+Dir*7000.f,ECC_Visibility,Q);
                    Note(FString::Printf(TEXT("finish view=%d pixel=%s hit=%s/%s at=%s"),S.View,*Pixel.ToString(),*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*H.ImpactPoint.ToString()));
                    if(auto* SM=Cast<UStaticMeshComponent>(H.GetComponent()))Note(TEXT("finish mesh ")+GetNameSafe(SM->GetStaticMesh()));
                }
            }
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir,V.Name),false,false);S.Step=Now;return;
        }
        S.Stage=1;
    }
    if(S.Stage==1)
    {
        int Glass=0,Eligible=0,Clear=0;bool OK=true;
        const float Radius=Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const float Standing=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const float Crouched=Pawn->GetCharacterMovement()->GetCrouchedHalfHeight()*Pawn->GetActorScale3D().Z;
        const auto* Move=Cast<UStoryMovementComponent>(Pawn->GetCharacterMovement());
        const float CrawlHalf=Move->CrawlCapsuleHalfHeight*Pawn->GetActorScale3D().Z;
        const float CrawlRadius=Move->CrawlCapsuleRadius*Pawn->GetActorScale3D().Z;
        TArray<AHouseReactiveFixture*> Windows;
        for(TActorIterator<AHouseReactiveFixture> It(W);It;++It)if(It->FixtureKind==EHouseReactiveFixtureKind::Window && !It->GetOwner())Windows.Add(*It);
        // Preserve positive controls before linked panes begin shattering.
        for(auto* G:Windows)
        {
            ++Glass;if(G->GetFixtureCollision()->GetCollisionEnabled()==ECollisionEnabled::NoCollision)OK=false;
        }
        for(auto* G:Windows)if(!G->IsBroken())
        {
            const FVector At=G->GetFixtureMesh()->Bounds.Origin;
            G->ApplyImpact(At,FVector::ForwardVector,8,PC);
            if(G->IsBroken())OK=false;
            G->ApplyImpact(At,FVector::ForwardVector,8,PC);
            if(G->IsBroken())OK=false;
            G->ApplyImpact(At,FVector::ForwardVector,8,PC);
            if(!G->IsBroken())OK=false;
        }
        for(auto* G:Windows)
        {
            if(G->GetFixtureCollision()->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)OK=false;
            if(auto* Frame=G->GetBreakawayFrameComponent())if(Frame->GetStaticMesh() && Frame->IsVisible())OK=false;
            const FBox B=G->GetFixtureMesh()->Bounds.GetBox();const int Axis=B.GetSize().X<B.GetSize().Y?0:1;
            float R=Radius;float Half=B.GetSize().Z>=Standing*2+8?Standing:Crouched;
            if(B.GetSize()[1-Axis]<R*2+6 || B.GetSize().Z<Half*2+6){R=CrawlRadius;Half=CrawlHalf;}
            if(B.GetSize()[1-Axis]<R*2+2 || B.GetSize().Z<Half*2+2)continue;
            ++Eligible;
            FVector N=FVector::ZeroVector;N[Axis]=1;
            FCollisionQueryParams Q(SCENE_QUERY_STAT(ConnectedWindowCapsule),false,Pawn);
            // Furniture can be moved by the player. Isolate permanent structure.
            for(TActorIterator<AHouseInteractiveProp> It(W);It;++It)Q.AddIgnoredActor(*It);
            bool Blocked=true;FHitResult H;FVector Lane=B.GetCenter();
            // Small transoms retain their outer sash and crossbar. Check the
            // actual openings above/below it using the normal crawl capsule.
            for(float ZOffset:{0.f,-1.f,1.f})
            {
                for(float Offset:{-.7f,.7f,0.f})
                {
                    FVector C=B.GetCenter();C[1-Axis]+=Offset*FMath::Max(0.f,B.GetSize()[1-Axis]*.5f-R-1.f);
                    C.Z+=ZOffset*FMath::Max(0.f,B.GetSize().Z*.5f-Half-1.f);
                    if(!W->SweepSingleByChannel(H,C-N*140.f,C+N*140.f,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(R,Half),Q)){Blocked=false;Lane=C;break;}
                }
                if(!Blocked)break;
            }
            Note(FString::Printf(TEXT("window %s clear=%d blocker=%s/%s"),*G->FixtureId.ToString(),!Blocked,*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent())));
            if(!Blocked)Note(TEXT("usable capsule lane ")+Lane.ToString());
            if(Blocked){Note(H.ImpactPoint.ToString()+TEXT(" normal ")+H.ImpactNormal.ToString());if(auto* SM=Cast<UStaticMeshComponent>(H.GetComponent()))Note(GetNameSafe(SM->GetStaticMesh()));}
            if(Blocked)OK=false;else++Clear;
        }
        Note(FString::Printf(TEXT("glass=%d eligible=%d clear=%d radius=%.1f standingHalf=%.1f crouchHalf=%.1f"),Glass,Eligible,Clear,Radius,Standing,Crouched));
        // Save even on failure so the complete list remains reviewable.
        FFileHelper::SaveStringToFile(S.Log,*FPaths::Combine(S.Dir,TEXT("window-audit.txt")));
        if(!OK){Finish(false);return;}
        const FVector P(3450,-1061,Standing+3);
        PC->SetViewTarget(Pawn);PC->SetControlRotation(FRotator(0,180,0));Pawn->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);S.Step=Now;S.Stage=2;
    }
    if(S.Stage==2 || S.Stage==3)
    {
        const float Goal=S.Stage==2?3110.f:3450.f;const float Direction=S.Stage==2?-1.f:1.f;
        // A window is entered by stepping/jumping over its retained sill.
        // Use the normal character jump, never teleport through the aperture.
        if(!S.Jumped && FMath::Abs(Pawn->GetActorLocation().X-3235.f)<150.f){Pawn->Jump();S.Jumped=true;}
        else if(S.Jumped && Now-S.Step>.7)Pawn->StopJumping();
        Pawn->AddMovementInput(FVector(Direction,0,0),1.f);
        if(Direction*(Pawn->GetActorLocation().X-Goal)>-12.f)
        {
            Pawn->GetCharacterMovement()->StopMovementImmediately();Note(S.Stage==2?TEXT("walked outside -> house through shattered side window"):TEXT("walked house -> outside through shattered side window"));
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir,S.Stage==2?TEXT("window-entered.png"):TEXT("window-exited.png")),false,false);
            ++S.Stage;S.Step=Now;S.Jumped=false;
        }
        else if(Now-S.Step>7){
            Note(Pawn->GetActorLocation().ToString());
            FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(WindowWalkingBlocker),false,Pawn);
            W->SweepSingleByChannel(H,Pawn->GetActorLocation(),Pawn->GetActorLocation()+FVector(Direction*60,0,0),FQuat::Identity,ECC_Pawn,
                FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),Q);
            Note(TEXT("walking blocker ")+GetNameSafe(H.GetActor())+TEXT("/")+GetNameSafe(H.GetComponent())+TEXT(" ")+H.ImpactPoint.ToString());
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir,TEXT("window-walk-blocked.png")),false,false);
            Finish(false);
        }
        return;
    }
    if(S.Stage==4 && Now-S.Step>2){Finish(true);}
}
#else
void TickHouseConnectionsReview(AStoryFirstPersonCharacter*){}
#endif
