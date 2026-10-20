// Explicit native model/pose diagnostic. No ambient flight/cadence claim.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifeBirds.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderWater.h"
#include "HouseExteriorSiteLayout.h"
#include "StoryCampaign.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "StaticMeshResources.h"

namespace HouseCalderWildlifeBirdArtImpl
{
bool Enabled(){static const bool B=(FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifeBirdArtReview")) || FParse::Param(FCommandLine::Get(),TEXT("CalderGoosePoseReview")));return B;}
const TCHAR* Name(int32 Kind){return Kind==0?TEXT("goose"):Kind==1?TEXT("crow"):Kind==2?TEXT("brown_bat"):TEXT("red_bat");}
}
void AHouseCalderWildlifeDirector::DrawBirdArt()
{
    if(!HouseCalderWildlifeBirdArtImpl::Enabled() || !Runtime || !Ready() || GetNetMode()==NM_DedicatedServer || BirdArtBuildAttempted)return;
    BirdArtBuildAttempted=true;
    const double Began=FPlatformTime::Seconds();auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Wildlife/M_CalderBirdPart.M_CalderBirdPart"));
    if(!Material){BirdArtBuildFailed=true;UE_LOG(LogTemp,Error,TEXT("CALDER_BIRD_ART_FAILED shared owned atlas material missing"));return;}
    for(int32 Kind=0;Kind<4;++Kind)
    {
        UStaticMesh* Mesh=HouseCalderWildlifeBirds::Build(this,Kind,Material);if(!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.Num()!=3){BirdArtBuildFailed=true;UE_LOG(LogTemp,Error,TEXT("CALDER_BIRD_ART_FAILED mesh kind=%d"),Kind);return;}
        for(int32 Lod=0;Lod<3;++Lod)
        {
            const auto& L=Mesh->GetRenderData()->LODResources[Lod];if((L.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords()<5 || L.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords()>8)){BirdArtBuildFailed=true;UE_LOG(LogTemp,Error,TEXT("CALDER_BIRD_ART_FAILED UV kind=%d lod=%d actual=%u vertices=%u"),Kind,Lod,L.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords(),L.GetNumVertices());return;}
            for(uint32 V=0;V<L.GetNumVertices();++V)
            {
                double Total=0.;for(int32 K=1;K<5;++K){const auto UV=L.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(V,K);if(UV.X<-.01 || UV.X>15.01 || UV.Y<-.001 || UV.Y>1.001){BirdArtBuildFailed=true;UE_LOG(LogTemp,Error,TEXT("CALDER_BIRD_ART_FAILED packed weights kind=%d lod=%d"),Kind,Lod);return;}Total+=UV.Y;}
                if(FMath::Abs(Total-1.)>.002){BirdArtBuildFailed=true;UE_LOG(LogTemp,Error,TEXT("CALDER_BIRD_ART_FAILED weight sum kind=%d lod=%d total=%.4f"),Kind,Lod,Total);return;}
            }
        }
        OwnedMeshes.Add(Mesh);auto* Part=NewObject<UInstancedStaticMeshComponent>(this,FName(*FString::Printf(TEXT("Wildlife_BirdArt_%d"),Kind)));AddInstanceComponent(Part);Part->SetupAttachment(RootComponent);
        Part->SetIsReplicated(false);Part->SetMobility(EComponentMobility::Movable);Part->SetStaticMesh(Mesh);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetGenerateOverlapEvents(false);Part->SetCanEverAffectNavigation(false);Part->SetCastShadow(false);Part->bAffectDistanceFieldLighting=false;Part->bAffectDynamicIndirectLighting=false;Part->SetHasPerInstancePrevTransforms(true);Part->SetNumCustomDataFloats(225);Part->SetCullDistances(0,40000);Part->AddInstance(FTransform::Identity,false);Part->SetVisibility(false);Part->RegisterComponent();BirdArtParts.Add(Part);
        const auto* Data=Mesh->GetRenderData();UE_LOG(LogTemp,Display,TEXT("CALDER_BIRD_ART_MESH species=%s lods=3 triangles=%u/%u/%u UV=%u usedUV=5 controls=16 customFloats=225 collision=0 runtimeSkeletons=0"),HouseCalderWildlifeBirdArtImpl::Name(Kind),Data->LODResources[0].GetNumTriangles(),Data->LODResources[1].GetNumTriangles(),Data->LODResources[2].GetNumTriangles(),Data->LODResources[0].VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords());
    }
    UE_LOG(LogTemp,Display,TEXT("CALDER_BIRD_ART_READY components=4 explicitModelSetupOnly=1 ambientFlightAccepted=0 buildMs=%.2f"),(FPlatformTime::Seconds()-Began)*1000.);
}
bool AHouseCalderWildlifeDirector::SetBirdArtForReview(int32 Kind,double Phase,const FTransform& Root,int32 GoosePoseMode)
{
    if(!HouseCalderWildlifeBirdArtImpl::Enabled() || BirdArtParts.Num()!=4 || Kind<0 || Kind>=4)return false;
    TArray<FTransform> Controls;
    if(GoosePoseMode>=0 && Kind==0)
    {
        const double Flaps[]={1.,0.,.35,.15,0.,0.,0.,0.},Folds[]={0.,0.,.1,.5,1.,1.,1.,1.},Necks[]={0.,0.,.45,.7,1.,.4,.6,1.};
        const int32 Mode=FMath::Clamp(GoosePoseMode,0,7);HouseCalderWildlifeBirds::Pose(0,Phase/HouseCalderWildlifeBirds::Frequency(0),Root,Controls,Mode==6?1.:0.,Flaps[Mode],Folds[Mode],Necks[Mode]);
    }
    else HouseCalderWildlifeBirds::Pose(Kind,Phase/HouseCalderWildlifeBirds::Frequency(Kind),Root,Controls,Kind<=1 && Phase>.7?1.:0.);
    if(Kind==0 && GoosePoseMode>=8)
    {
        HouseCalderWildlifeBirds::Pose(0,Phase/HouseCalderWildlifeBirds::Frequency(0),Root,Controls,GoosePoseMode%4==3?1.:0.,0.,1.,1.);FVector Soles[2],Normals[2];
        for(int32 Leg=0;Leg<2;++Leg)
        {
            FVector P=Root.TransformPosition(FVector(-17.,Leg==0?5.:-5.,0.));const double Z=HouseCalderTerrain::SurfaceZ(P.X,P.Y),DX=(HouseCalderTerrain::SurfaceZ(P.X+10.,P.Y)-HouseCalderTerrain::SurfaceZ(P.X-10.,P.Y))/20.,DY=(HouseCalderTerrain::SurfaceZ(P.X,P.Y+10.)-HouseCalderTerrain::SurfaceZ(P.X,P.Y-10.))/20.;
            const double Lift=(GoosePoseMode%4==1&&Leg==0)||(GoosePoseMode%4==2&&Leg==1)?4.:0.;Soles[Leg]=FVector(P.X,P.Y,Z+Lift);Normals[Leg]=FVector(-DX,-DY,1.).GetSafeNormal();
        }
        HouseCalderWildlifeBirds::GroundLegs(Root,Soles,Normals,Controls);
    }
    TArray<float> Data;Data.SetNumZeroed(225);HouseCalderWildlifeBirds::Pack(Controls,Data.GetData());HouseCalderWildlifeBirds::Pack(Controls,Data.GetData()+112);Data[224]=float(Root.GetScale3D().X);
    for(int32 I=0;I<4;++I)
    {
        auto* Part=BirdArtParts[I].Get();Part->SetVisibility(I==Kind);if(I!=Kind)continue;
        const TArray<FTransform> Current={Root},Previous={Root};Part->BatchUpdateInstancesTransforms(0,Current,Previous,false,false,true);Part->SetCustomData(0,Data,false);Part->UpdateBounds();Part->MarkRenderTransformDirty();
        if(Kind==0 && GoosePoseMode>=8)
        {
            const auto& Actual=Part->PerInstanceSMCustomData;if(Actual.Num()!=225){BirdArtBuildFailed=true;return false;}const auto& L=Part->GetStaticMesh()->GetRenderData()->LODResources[0];double Worst=0.;int32 Count=0;
            for(uint32 V=0;V<L.GetNumVertices();++V)
            {
                const FVector Local(L.VertexBuffers.PositionVertexBuffer.VertexPosition(V));const auto Weight=L.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(V,1);const int32 Joint=FMath::RoundToInt(Weight.X);if((Joint!=11&&Joint!=13)||Local.Z> -22.20 || Weight.Y<.99)continue;
                const int32 Leg=Joint==11?0:1;const double Lift=(GoosePoseMode%4==1&&Leg==0)||(GoosePoseMode%4==2&&Leg==1)?4.:0.;const int32 Offset=Joint*7;const FQuat Q(Actual[Offset],Actual[Offset+1],Actual[Offset+2],Actual[Offset+3]);const FVector World=Q.RotateVector(Local*Actual[224])+FVector(Actual[Offset+4],Actual[Offset+5],Actual[Offset+6]);
                Worst=FMath::Max(Worst,FMath::Abs(World.Z-HouseCalderTerrain::SurfaceZ(World.X,World.Y)-Lift));++Count;
            }
            UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_GROUND_SOLES mode=%d samples=%d worstSurfaceErrorCm=%.6f actualSubmittedISMData=1 fixedPoseOnly=1 movingFeetAccepted=0"),GoosePoseMode,Count,Worst);
            if(Count<20 || Worst>3.){BirdArtBuildFailed=true;return false;}
        }

    }
    return true;
}
namespace HouseCalderWildlifeBirdArtReviewImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ACameraActor> Camera;int32 Stage=0,Phase=0;double Began=0.,StageAt=0.;FVector At=FVector::ZeroVector;FString Dir;TArray<FString> Captures;
};
FState S;
bool GroundPoses(){static const bool B=FParse::Param(FCommandLine::Get(),TEXT("CalderGooseGroundPoseReview"));return B;}
bool GoosePoses(){static const bool Value=FParse::Param(FCommandLine::Get(),TEXT("CalderGoosePoseReview"));return Value;}
void Finish(bool Passed,const FString& Why)
{
    for(const FString& Capture:S.Captures)Passed&=IFileManager::Get().FileSize(*Capture)>0;
    FString Result=FString::Printf(TEXT("%s scope=%s captures=%d expected=%d eyeCm=170 collision=0 runtimeSkeletons=0 additionalLights=0 ambientFlightAcceptance=0 wholePhase3Complete=0 reason=%s\n"),Passed?TEXT("PASS"):TEXT("FAIL"),GroundPoses()?TEXT("explicitNativeGooseGroundFixedPoses"):GoosePoses()?TEXT("explicitNativeGooseFixedPoses"):TEXT("explicitNativeBirdModelsAndFixedPoses"),S.Captures.Num(),GoosePoses()?8:16,*Why);
    FFileHelper::SaveStringToFile(Result,*(S.Dir/TEXT("result.txt")));UE_LOG(LogTemp,Display,TEXT("CALDER_BIRD_ART_REVIEW %s"),*Result);S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
bool Eye(UWorld* World,AActor* Pawn,const FVector& Target,int32 Kind,int32 Shot,FVector& Out)
{
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(CalderBirdArtStanding),true,Pawn);const double Distance=Kind==0?350.:Kind==1?240.:105.;
    for(int32 Attempt=0;Attempt<36;++Attempt)
    {
        const double Yaw=180.+(Shot>=2?15.:65.)+Attempt*10.;FVector P=Target+FRotator(0,Yaw,0).Vector()*Distance;const double Z=HouseExteriorSite::GroundHeight(P.X,P.Y);FHitResult Hit;
        if(GroundPoses()){float Water=0.f;if(HouseCalderWater::WaterAt(FVector2D(P),Water))continue;}
        if(!World->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,Z+150.),FVector(P.X,P.Y,Z-150.),ECC_Visibility,Query) || FMath::Abs(Hit.ImpactPoint.Z-Z)>3. || Hit.ImpactNormal.Z<.766)continue;
        P.Z=Hit.ImpactPoint.Z+170.;if(World->LineTraceSingleByChannel(Hit,P,Target,ECC_Visibility,Query))continue;Out=P;return true;
    }
    return false;
}
void Tick(UWorld* World,ELevelTick,float)
{
    if(!HouseCalderWildlifeBirdArtImpl::Enabled() || !World || !World->IsGameWorld() || !World->HasBegunPlay() || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    const double Now=World->GetTimeSeconds();if(S.World.Get()!=World){S=FState();S.World=World;S.Began=Now;S.Dir=FPaths::ProjectSavedDir()/TEXT("CalderWildlifeBirdArtReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
    if(S.Phase<0)return;if(Now-S.Began>150.){Finish(false,TEXT("native art review timed out"));return;}
    auto* PC=World->GetFirstPlayerController();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;TActorIterator<AHouseCalderWildlifeDirector> It(World);auto* D=It?*It:nullptr;if(!Pawn || !D || !D->Ready())return;
    if(D->BirdArtFailedForReview()){Finish(false,TEXT("model build or packed UV validation failed; see native log"));return;}
    if(S.Phase==0)
    {
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->DisableMovement();Pawn->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);if(AHUD* HUD=PC->GetHUD())HUD->bShowHUD=false;
        const auto& Nodes=D->HabitatNodes(1);if(Nodes.IsEmpty())return;S.At=Nodes[0];for(const FVector& P:Nodes)if(FVector::DistSquaredXY(P,FVector(-26000,6000,0))<FVector::DistSquaredXY(S.At,FVector(-26000,6000,0)))S.At=P;
        if(GroundPoses())S.At=FVector(25200,-10800,HouseCalderTerrain::SurfaceZ(25200,-10800));S.At.Z+=GroundPoses()?27.2:250.;FActorSpawnParameters Params;Params.Owner=D->GetOwner();S.Camera=World->SpawnActor<ACameraActor>(Params);if(!S.Camera.IsValid()){Finish(false,TEXT("camera spawn failed"));return;}S.Camera->GetCameraComponent()->SetFieldOfView(40.f);S.Phase=1;
    }
    if(S.Phase==1)
    {
        const int32 Kind=GoosePoses()?0:S.Stage/4,Shot=GoosePoses()?S.Stage:S.Stage%4;const double Phases[]={0.,.25,.5,.75};const double Scale=GroundPoses()?(Shot==6?.92:Shot==7?1.08:1.):1.;FVector At=S.At;if(GroundPoses())At.Z=HouseCalderTerrain::SurfaceZ(At.X,At.Y)+27.2*Scale;const FTransform Root(FRotator(!GroundPoses()&&GoosePoses()&&Shot==2?18.:0.,GroundPoses()&&Shot>=4?90.:GoosePoses()&&Shot==7?0.:180.,0.),At,FVector(Scale));
        if(!D->SetBirdArtForReview(Kind,Phases[Shot%4],Root,GoosePoses()?(GroundPoses()?Shot+8:Shot):-1))return;
        FVector Position;if(!Eye(World,Pawn,S.At,Kind,Shot,Position)){Finish(false,TEXT("no clear physical170cm eye"));return;}
        S.Camera->GetCameraComponent()->SetFieldOfView(Kind==0?40.f:45.f);S.Camera->SetActorLocationAndRotation(Position,(S.At+FVector(0,0,Kind==1?18.:0.)-Position).Rotation());PC->SetViewTarget(S.Camera.Get());D->SetTimingView(FName(*FString::Printf(TEXT("bird_art_%s"),HouseCalderWildlifeBirdArtImpl::Name(Kind))));S.StageAt=Now;S.Phase=2;return;
    }
    if(S.Phase==2 && Now-S.StageAt>1.5 && !FScreenshotRequest::IsScreenshotRequested())
    {
        const FString Stem=GroundPoses()?FString::Printf(TEXT("detail_goose_ground_%02d.png"),S.Stage):GoosePoses()?FString::Printf(TEXT("detail_goose_pose_%s_%02d.png"),HouseCalderWildlifeBirdArtImpl::Name(0),S.Stage):FString::Printf(TEXT("detail_bird_%s_%02d.png"),HouseCalderWildlifeBirdArtImpl::Name(S.Stage/4),S.Stage%4);const FString File=S.Dir/Stem;GetHighResScreenshotConfig().SetHDRCapture(false);FScreenshotRequest::RequestScreenshot(File,false,false);S.Captures.Add(File);S.Phase=3;S.StageAt=Now;return;
    }
    if(S.Phase==3 && Now-S.StageAt>1. && !FScreenshotRequest::IsScreenshotRequested())
    {if(++S.Stage>=(GoosePoses()?8:16)){Finish(true,TEXT("native captures produced; individual visual inspection still required"));return;}S.Phase=1;}
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}} Registration;
}
#endif
