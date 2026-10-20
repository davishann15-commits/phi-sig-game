// Unbound development-only tree candidate preview. Default game unchanged.
#if !UE_BUILD_SHIPPING
#include "HouseCalderTreeMeshes.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "StaticMeshResources.h"

namespace HouseCalderTreePreviewImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ACameraActor> Camera;
    TWeakObjectPtr<AStaticMeshActor> Tree;
    TWeakObjectPtr<UStaticMesh> Prepared;
    bool PreparedBeech=false;
    FString Directory;
    TArray<FString> Captures;
    double Next=0.;
    int32 Shot=0;
    bool Done=false, Safe=true, PendingCapture=false;
};
FState State;
void Tick(UWorld* World,ELevelTick,float)
{
    if (!World || !World->IsGameWorld() || !FParse::Param(FCommandLine::Get(),TEXT("CalderTreePreview"))
        || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House")) return;
    if (State.World.Get()!=World) { State=FState(); State.World=World; State.Next=World->GetTimeSeconds()+8.; }
    if (State.Done || World->GetTimeSeconds()<State.Next) return;
    APlayerController* PC=World->GetFirstPlayerController();
    if (!PC) return;
    if (!State.Camera.IsValid())
    {
        State.Directory=FPaths::ProjectSavedDir()/TEXT("CalderTreePreview");
        IFileManager::Get().MakeDirectory(*State.Directory,true);
        FActorSpawnParameters Spawn; Spawn.ObjectFlags|=RF_Transient;
        for (TActorIterator<AActor> It(World);It;++It)
            if (It->GetName().StartsWith(TEXT("HouseExteriorDetail"))) { Spawn.Owner=*It; break; }
        State.Camera=World->SpawnActor<ACameraActor>(Spawn);
        State.Tree=World->SpawnActor<AStaticMeshActor>(Spawn);
        if (!State.Camera.IsValid() || !State.Tree.IsValid()) { State.Safe=false; return; }
        UStaticMeshComponent* Part=State.Tree->GetStaticMeshComponent();
        Part->SetMobility(EComponentMobility::Movable);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false);
        Part->SetCastShadow(false);
        Part->bAffectDistanceFieldLighting=false;
        Part->bAffectDynamicIndirectLighting=false;
        Part->SetForceDisableNanite(true);
        PC->SetViewTarget(State.Camera.Get());
        if (AHUD* Hud=PC->GetHUD()) Hud->bShowHUD=false;
        State.Camera->GetCameraComponent()->SetFieldOfView(65.f);
    }
    const bool DistanceViews=FParse::Param(FCommandLine::Get(),TEXT("CalderTreeDistanceViews"));
    const int32 ViewsPerTree=DistanceViews?5:4;
    const int32 TotalViews=ViewsPerTree*2;
    if (State.Shot<TotalViews)
    {
        const bool bBeech=State.Shot<ViewsPerTree;
        const TCHAR* Label=bBeech?TEXT("Beech"):TEXT("Maple");
        const FString Path=FString::Printf(TEXT("/Game/SeniorSendOff/Calder/Trees/SM_CalderTreeCandidate_%s.SM_CalderTreeCandidate_%s"),Label,Label);
        const bool bOriginalDonor=FParse::Param(FCommandLine::Get(),TEXT("CalderTreeOriginalDonor"));
        FString ActualPath=Path;
        if (bOriginalDonor)
            ActualPath=bBeech?TEXT("/Game/EuropeanBeech/Geometry/SimpleWind/SM_EuropeanBeech_Field_01.SM_EuropeanBeech_Field_01"):
                TEXT("/Game/NorwayMaple/Geometry/SimpleWind/SM_NorwayMaple_Field_01.SM_NorwayMaple_Field_01");
        UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr,*ActualPath);
        if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.Num()<3)
        {
            State.Safe=false; State.Shot=TotalViews; return;
        }
        UStaticMeshComponent* Part=State.Tree->GetStaticMeshComponent();
        if (!bOriginalDonor && FParse::Param(FCommandLine::Get(),TEXT("CalderTreeRetainStructure")))
        {
            if (!State.Prepared.IsValid() || State.PreparedBeech!=bBeech)
            {
                const bool ReducedWood=FParse::Param(FCommandLine::Get(),TEXT("CalderTreeReducedWood"));
                const FString StructuralPath=ReducedWood?
                    FString::Printf(TEXT("/Game/SeniorSendOff/Calder/Trees/SM_CalderTreeWood_%s.SM_CalderTreeWood_%s"),Label,Label):
                    FString::Printf(TEXT("/Game/SeniorSendOff/Calder/Trees/SM_CalderTreeCandidate_%s_Structural.SM_CalderTreeCandidate_%s_Structural"),Label,Label);
                UStaticMesh* Structural=LoadObject<UStaticMesh>(nullptr,*StructuralPath);
                float NearGrid=2.f,MiddleGrid=12.f,FarGrid=24.f;
                FParse::Value(FCommandLine::Get(),TEXT("CalderTreeWoodGrid0="),NearGrid);
                FParse::Value(FCommandLine::Get(),TEXT("CalderTreeWoodGrid1="),MiddleGrid);
                FParse::Value(FCommandLine::Get(),TEXT("CalderTreeWoodGrid2="),FarGrid);
                State.Prepared=HouseCalderTreeMeshes::RetainStructure(State.Tree.Get(),Mesh,Structural,NearGrid,MiddleGrid,FarGrid);
                State.PreparedBeech=bBeech;
            }
            Mesh=State.Prepared.Get();
            if (!Mesh) { State.Safe=false; State.Shot=TotalViews; return; }
        }
        Part->SetStaticMesh(Mesh);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        const FBox Bounds=Mesh->GetBoundingBox();
        const float Scale=1600.f/FMath::Max(1.f,float(Bounds.GetSize().Z));
        const FVector Base(0,-50000,5000);
        State.Tree->SetActorScale3D(FVector(Scale));
        State.Tree->SetActorLocation(Base-FVector(0,0,Bounds.Min.Z*Scale));
        const int32 View=State.Shot%ViewsPerTree;
        const int32 ForcedLod=View==1?2:(View==2 || View==4)?3:1;
        Part->SetForcedLodModel(ForcedLod);
        FVector Target=Base+FVector(0,0,800);
        FVector Eye=Base+FVector(View==2?-2500:2500,-1500,170);
        if (View==4) Eye=Base+FVector(14000,-5400,170);
        if (View==3) { Target=Base+FVector(150,0,1000); Eye=Target+FVector(550,-700,0); }
        State.Camera->SetActorLocationAndRotation(Eye,(Target-Eye).Rotation());
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
        const FString Capture=State.Directory/FString::Printf(TEXT("%s_%d.png"),Label,View);
        if (State.PendingCapture)
        {
            IFileManager::Get().Delete(*Capture);
            FScreenshotRequest::RequestScreenshot(Capture,false,false);
            State.Captures.Add(Capture);
            State.PendingCapture=false;
            ++State.Shot; State.Next=World->GetTimeSeconds()+.2; return;
        }
        // Give the new mesh, texture residency and selected LOD three seconds
        // to settle before requesting a raw frame.
        State.PendingCapture=true;
        const auto& Lods=Mesh->GetRenderData()->LODResources;
        UE_LOG(LogTemp,Display,TEXT("CALDER_TREE_PREVIEW label=%s view=%d triangles=%d/%d/%d heightCm=1600 scale=%.6f collision=0 defaultBinding=0 originalDonor=%d path=%s eyeAboveRootCm=%.1f distanceCm=%.1f forcedLod=%d"),
            Label,View,Lods[0].GetNumTriangles(),Lods[1].GetNumTriangles(),Lods[2].GetNumTriangles(),Scale,int32(bOriginalDonor),*Capture,Eye.Z-Base.Z,FVector::Dist(Eye,Target),ForcedLod);
        State.Next=World->GetTimeSeconds()+3.; return;
    }
    for (const FString& Path:State.Captures) State.Safe &= IFileManager::Get().FileSize(*Path)>0;
    State.Safe &= State.Captures.Num()==TotalViews;
    State.Done=true;
    const FString Result=FString::Printf(TEXT("%s candidate previews=%d defaultBinding=0 appearanceNotAccepted=1\n"),State.Safe?TEXT("PASS"):TEXT("FAIL"),State.Captures.Num());
    FFileHelper::SaveStringToFile(Result,*(State.Directory/TEXT("result.txt")));
    UE_LOG(LogTemp,Display,TEXT("CALDER_TREE_PREVIEW_REVIEW %s"),*Result);
    FPlatformMisc::RequestExitWithStatus(false,State.Safe?0:1);
}
struct FRegistration { FRegistration() { FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick); } };
FRegistration Registration;
}
#endif
