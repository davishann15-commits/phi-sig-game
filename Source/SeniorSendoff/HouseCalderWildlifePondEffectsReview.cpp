// Native effect pools and actual transformed effect vertices at the original
// pond/shore. Offset is explicit; clock advances at actual server rate.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifeGooseShore.h"
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
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "StaticMeshResources.h"
namespace HouseCalderWildlifePondEffectsReviewImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ACameraActor> Camera;HouseCalderWildlifeSkyPaths::FPlan Sky;HouseCalderWildlifeGoosePaths::FPlan Pond;HouseCalderWildlifeGooseShore::FPlan Shore;
    bool Ready=false,Done=false,Entry=false,Close=false;double Began=0.,Next=0.,SurfaceError=0.,GroundError=0.,EntryBirth=0.;int32 Shot=0,Samples=0,Peaks[4]={},Vertices[4]={};FString Dir;TArray<FString> Captures;
};FState S;
void Finish(bool Passed,const FString& Why)
{
    S.Done=true;for(const auto& File:S.Captures)Passed&=IFileManager::Get().FileSize(*File)>1000;
    const FString Result=FString::Printf(TEXT("%s scope=nativeGooseWaterEffects mode=%s captures=%d expected=8 samples=%d peakContact=%d peakWakes=%d peakDrops=%d peakRings=%d contactVertices=%d wakeVertices=%d dropVertices=%d ringVertices=%d maximumGroundErrorCm=%.6f maximumWaterPlaneErrorCm=%.6f components=4 fixedPools=16/32/16/16 actualSubmittedTransforms=1 actualCustomData=1 newLights=0 collision=0 runtimeSkeletons=0 realServerRate=1 diagnosticOffset=1 audioAccepted=0 fullPerformanceAccepted=0 wholePhase3Complete=0 reason=%s\n"),Passed?TEXT("PASS"):TEXT("FAIL"),S.Entry?TEXT("shoreEntry"):TEXT("arrival"),S.Captures.Num(),S.Samples,S.Peaks[0],S.Peaks[1],S.Peaks[2],S.Peaks[3],S.Vertices[0],S.Vertices[1],S.Vertices[2],S.Vertices[3],S.GroundError,S.SurfaceError,*Why);
    FFileHelper::SaveStringToFile(Result,*(S.Dir/TEXT("result.txt")));UE_LOG(LogTemp,Display,TEXT("CALDER_POND_EFFECT_REVIEW %s"),*Result);FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
void Tick(UWorld* W,ELevelTick,float)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CalderGooseEffectsReview"))||!W||!W->IsGameWorld()||!W->HasBegunPlay()||UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House"))return;
    const double Wall=FPlatformTime::Seconds();if(S.World.Get()!=W){S=FState();S.World=W;S.Began=Wall;S.Entry=FParse::Param(FCommandLine::Get(),TEXT("CalderGooseEffectsEntryReview"));S.Dir=FPaths::ProjectSavedDir()/TEXT("CalderGooseEffectsReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
    if(S.Done)return;if(Wall-S.Began>180.){Finish(false,TEXT("effect review timeout"));return;}
    auto* PC=W->GetFirstPlayerController();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;TActorIterator<AHouseCalderWildlifeDirector> It(W);auto* D=It?*It:nullptr;if(!Pawn||!D||!D->Ready()||!D->PondReadyForReview())return;
    UInstancedStaticMeshComponent* Parts[4]={};TInlineComponentArray<UInstancedStaticMeshComponent*> All(D);for(auto* P:All)for(int32 K=0;K<4;++K)if(P->GetName()==FString::Printf(TEXT("Wildlife_PondEffect_%d"),K))Parts[K]=P;for(auto* P:Parts)if(!P)return;
    const double Raw=HouseCalderWildlife::CycleTime(W);
    if(!S.Ready)
    {
        if(!S.Sky.Build(W)||!S.Pond.Build(W,S.Sky)||!S.Shore.Build(W,S.Sky,S.Pond)||D->ShoreHashForReview()!=S.Shore.Hash||D->RenderComponentCount()!=13){Finish(false,TEXT("effect/reference/component mismatch"));return;}
        const int32 Counts[4]={16,32,16,16};for(int32 K=0;K<4;++K)if(Parts[K]->GetInstanceCount()!=Counts[K]||Parts[K]->GetCollisionEnabled()!=ECollisionEnabled::NoCollision||Parts[K]->GetGenerateOverlapEvents()||Parts[K]->CanEverAffectNavigation()||Parts[K]->CastShadow||Parts[K]->GetStaticMesh()->GetRenderData()->LODResources.Num()!=3){Finish(false,TEXT("pool/component configuration mismatch"));return;}
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->DisableMovement();Pawn->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);if(auto* H=PC->GetHUD())H->bShowHUD=false;
        FActorSpawnParameters Params;Params.Owner=D->GetOwner();S.Camera=W->SpawnActor<ACameraActor>(Params);if(!S.Camera.IsValid()){Finish(false,TEXT("no native effect camera"));return;}
        S.Close=FParse::Param(FCommandLine::Get(),TEXT("CalderGooseEffectsCloseReview"));
        const FVector Eye=S.Entry?FVector(25544.683,-10860.777,HouseExteriorSite::GroundHeight(25544.683,-10860.777)+170.):S.Close?S.Pond.Touches[0]+FVector(0,-900,170):FVector(25200,-16420,890);
        S.Camera->SetActorLocation(Eye);S.Camera->GetCameraComponent()->SetFieldOfView(S.Entry||S.Close?65.f:45.f);PC->SetViewTarget(S.Camera.Get());D->SetTimingView(S.Entry?TEXT("goose_water_shore"):S.Close?TEXT("goose_water_arrival_close"):TEXT("goose_water_arrival"));
        // Capture the actual first wet steps, rather than only the start of the
        // longer bank-to-home return. This mirrors the renderer's wet boundary.
        const auto& Path=S.Shore.Routes[0];double Exit=0.;
        for(int32 I=1;I<Path.Points.Num();++I)
        {float A=0.f,B=0.f;if(!HouseCalderWater::WaterAt(FVector2D(Path.Points[I-1]),A)||HouseCalderWater::WaterAt(FVector2D(Path.Points[I]),B))continue;double Lo=Path.Arc[I-1],Hi=Path.Arc[I];for(int32 J=0;J<18;++J){const double Mid=(Lo+Hi)*.5;float Water=0.f;if(HouseCalderWater::WaterAt(FVector2D(HouseCalderWildlifeSkyPaths::Along(Path,Mid)),Water))Lo=Mid;else Hi=Mid;}Exit=(Lo+Hi)*.5;}
        S.EntryBirth=S.Pond.SplitScene+S.Pond.Durations[0]-6.-Exit/35.+.65;S.Ready=true;
    }
    const double First=S.Pond.SplitScene+S.Pond.Durations[0],Last=S.Pond.SplitScene+S.Pond.Durations[7];
    HouseCalderWildlifeGoosePaths::FCycleSample Look;S.Shore.Cycle(S.Pond,Raw,S.Entry?0:1,S.Entry?6:0,Look);S.Camera->SetActorRotation((Look.Bird.Position-S.Camera->GetActorLocation()).Rotation());
    if(Wall>=S.Next)
    {
        S.Next=Wall+.1;++S.Samples;
        for(int32 Kind=0;Kind<4;++Kind)
        {
            int32 Shown=0;const auto* Part=Parts[Kind];const auto& L=Part->GetStaticMesh()->GetRenderData()->LODResources[0];
            for(int32 Id=0;Id<Part->GetInstanceCount();++Id)
            {
                FTransform Root;if(!Part->GetInstanceTransform(Id,Root,true)){Finish(false,TEXT("missing actual effect transform"));return;}if(Root.GetScale3D().IsNearlyZero())continue;++Shown;if(!Part->Bounds.GetBox().IsInsideOrOn(Root.GetTranslation())){Finish(false,TEXT("active effect outside native bounds"));return;}
                if(Kind>0&&(Part->NumCustomDataFloats!=4||Part->PerInstanceSMCustomData.Num()<4*(Id+1)||Part->PerInstanceSMCustomData[Id*4]<=.001)){Finish(false,TEXT("visible effect missing actual alpha data"));return;}
                for(uint32 V=0;V<L.GetNumVertices();++V)
                {
                    const FVector P=Root.TransformPosition(FVector(L.VertexBuffers.PositionVertexBuffer.VertexPosition(V)));float Water=0.f,Deck=0.f;
                    if(Kind==0)S.GroundError=FMath::Max(S.GroundError,FMath::Abs(P.Z-HouseCalderTerrain::SurfaceZ(P.X,P.Y)));
                    else if(!HouseCalderWater::WaterAt(FVector2D(P),Water)||HouseCalderWater::DeckAt(FVector2D(P),Deck)){Finish(false,FString::Printf(TEXT("submitted effect vertex outside water/deck-free footprint kind=%d id=%d at=%s"),Kind,Id,*P.ToString()));return;}
                    else if(Kind!=2)S.SurfaceError=FMath::Max(S.SurfaceError,FMath::Abs(P.Z-Water));++S.Vertices[Kind];
                }
            }
            S.Peaks[Kind]=FMath::Max(S.Peaks[Kind],Shown);
        }
        if(S.GroundError>3.||S.SurfaceError>4.){Finish(false,TEXT("contact/water plane placement beyond unchanged3cm ground or4cm effect plane limit"));return;}
    }
    const double ArrivalTimes[8]={First-.7,First+.25,First+4.25,First+8.25,Last+.25,Last+2.,Last+5.,Last+8.};
    const double EntryTimes[8]={S.EntryBirth-.7,S.EntryBirth+.15,S.EntryBirth+.35,S.EntryBirth+.65,S.EntryBirth+1.,S.EntryBirth+1.5,S.EntryBirth+4.,S.EntryBirth+8.};const double* Times=S.Entry?EntryTimes:ArrivalTimes;
    if(S.Shot<8&&Raw>=Times[S.Shot]&&!FScreenshotRequest::IsScreenshotRequested())
    {const FString File=S.Dir/FString::Printf(TEXT("detail_goose_water_%02d.png"),S.Shot++);GetHighResScreenshotConfig().SetHDRCapture(false);FScreenshotRequest::RequestScreenshot(File,false,false);S.Captures.Add(File);}
    if(S.Shot==8&&Raw>Times[7]+2.&&!FScreenshotRequest::IsScreenshotRequested())Finish(S.Peaks[1]>0&&S.Peaks[2]>0&&S.Peaks[3]>0&&(!S.Entry||S.Peaks[0]>0),TEXT("native pool/geometry files produced;inspectall8images;audio/fullperformance/defaultpromotionremain"));
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}}Registration;
}
#endif
