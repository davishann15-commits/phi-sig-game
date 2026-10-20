// Development-only assembly/capture probe. This complements the physical pig
// smokes; it never treats a screenshot as a locomotion or contact verdict.
#if !UE_BUILD_SHIPPING
#include "HouseCalderLandscape.h"
#include "HouseRoamingPig.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StaticMeshResources.h"

namespace HouseCalderPigVisualReviewImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ACameraActor> Camera;
    TArray<TWeakObjectPtr<AHouseRoamingPig>> Pigs;
    double Start = 0.0, Next = 0.0;
    int32 Shot = 0;
    bool Done = false, Safe = true;
    FString Dir;
    TArray<FString> Captures;
};
FState& State() { static FState Value; return Value; }

void Tick(UWorld* World, ELevelTick, float)
{
    if (!World || !World->IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("CalderPigVisualReview"))
        || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    FState& S = State();
    const double Now = World->GetTimeSeconds();
    if (S.World.Get() != World) { S = FState(); S.World = World; S.Start = Now; S.Next = Now + 8.0; }
    if (S.Done || Now < S.Next) return;
    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC || !PC->GetPawn()) return;
    if (!S.Camera.IsValid())
    {
        S.Dir = FPaths::ProjectSavedDir() / TEXT("CalderPigVisualReview");
        IFileManager::Get().MakeDirectory(*S.Dir, true);
        FActorSpawnParameters Spawn;
        Spawn.ObjectFlags |= RF_Transient;
        S.Camera = World->SpawnActor<ACameraActor>(Spawn);
        if (!S.Camera.IsValid()) return;
        PC->SetViewTarget(S.Camera.Get());
        if (AHUD* Hud = PC->GetHUD()) Hud->bShowHUD = false;
        S.Camera->GetCameraComponent()->SetFieldOfView(65.f);
        TArray<TSharedPtr<FJsonValue>> Pigs;
        for (TActorIterator<AHouseRoamingPig> It(World); It; ++It)
        {
            S.Pigs.Add(*It);
            TSharedPtr<FJsonObject> Pig = MakeShared<FJsonObject>();
            Pig->SetStringField(TEXT("name"), It->PigName);
            Pig->SetStringField(TEXT("feet"), It->FeetLocation().ToString());
            Pig->SetStringField(TEXT("roomMin"), It->PigstyMinimum.ToString());
            Pig->SetStringField(TEXT("roomMax"), It->PigstyMaximum.ToString());
            TArray<TSharedPtr<FJsonValue>> Nodes;
            for (const FHousePigRoamNode& Node : It->RoamGraph) Nodes.Add(MakeShared<FJsonValueString>(Node.Feet.ToString()));
            Pig->SetArrayField(TEXT("nodes"), Nodes);
            TArray<TSharedPtr<FJsonValue>> Parts;
            int32 CalderParts = 0;
            TInlineComponentArray<UStaticMeshComponent*> Components(*It);
            for (UStaticMeshComponent* Component : Components)
            {
                UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
                if (!Mesh) continue;
                if (Mesh->GetName().StartsWith(TEXT("SM_CalderPig_")))
                {
                    ++CalderParts;
                    S.Safe &= Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources.Num() == 3;
                    if (Component->GetFName()==TEXT("PigBody") && HouseCalderLandscape::Enabled() && Mesh->GetRenderData())
                    {
                        const FStaticMeshVertexBuffer& UVs=Mesh->GetRenderData()->LODResources[0].VertexBuffers.StaticMeshVertexBuffer;
                        uint32 Mask=0; float SumError=0.f;
                        S.Safe &= UVs.GetNumTexCoords()>=5;
                        for (uint32 Vertex=0;Vertex<UVs.GetNumVertices() && UVs.GetNumTexCoords()>=5;++Vertex)
                        {
                            float Sum=0.f;
                            for (uint32 Influence=1;Influence<=4;++Influence)
                            {
                                const FVector2f Pair=UVs.GetVertexUV(Vertex,Influence);
                                const int32 Joint=FMath::RoundToInt(Pair.X);
                                S.Safe &= Joint>=0 && Joint<12 && Pair.Y>=-.001f && Pair.Y<=1.001f;
                                if (Pair.Y>.001f && Joint>=0 && Joint<12) Mask|=1u<<Joint;
                                Sum+=Pair.Y;
                            }
                            SumError=FMath::Max(SumError,FMath::Abs(1.f-Sum));
                        }
                        S.Safe &= UVs.GetNumVertices()>0 && Mask==4095 && SumError<.005f;
                        Pig->SetNumberField(TEXT("jointBlendVertices"),UVs.GetNumVertices());
                        Pig->SetNumberField(TEXT("jointBlendMask"),Mask);
                        Pig->SetNumberField(TEXT("weightSumError"),SumError);
                        UE_LOG(LogTemp,Display,TEXT("CALDER_PIG_SKIN_DATA name=%s vertices=%u jointMask=%u weightSumError=%.6f"),*It->PigName,UVs.GetNumVertices(),Mask,SumError);
                    }
                }
                Parts.Add(MakeShared<FJsonValueString>(Component->GetName() + TEXT(":") + Mesh->GetPathName()));
            }
            S.Safe &= !HouseCalderLandscape::Enabled() || CalderParts == 12;
            Pig->SetArrayField(TEXT("parts"), Parts);
            Pigs.Add(MakeShared<FJsonValueObject>(Pig));
            if (FParse::Param(FCommandLine::Get(), TEXT("CalderPigDonor")))
            {
                USceneComponent* VisualRoot = nullptr;
                TInlineComponentArray<USceneComponent*> SceneComponents(*It);
                for (USceneComponent* Component : SceneComponents)
                    if (Component && Component->GetFName() == TEXT("PigVisualRoot")) VisualRoot = Component;
                UStaticMesh* Donor = LoadObject<UStaticMesh>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Pig/SM_CalderPig_Donor.SM_CalderPig_Donor"));
                S.Safe &= Donor && VisualRoot;
                if (Donor && VisualRoot)
                {
                    for (UStaticMeshComponent* Component : Components) Component->SetVisibility(false);
                    UStaticMeshComponent* Control = NewObject<UStaticMeshComponent>(*It,NAME_None,RF_Transient);
                    It->AddInstanceComponent(Control);
                    Control->SetupAttachment(VisualRoot);
                    Control->SetStaticMesh(Donor);
                    Control->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    Control->RegisterComponent();
                }
            }
        }
        S.Safe &= S.Pigs.Num() == 2;
        TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
        Report->SetArrayField(TEXT("pigs"), Pigs);
        Report->SetBoolField(TEXT("assemblyPassed"), S.Safe);
        Report->SetStringField(TEXT("coverage"), TEXT("Static-part assembly and diagnostic captures only; physical smokes remain required."));
        FString Json;
        FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json, *(S.Dir / TEXT("assembly.json")));
    }
    if (S.Shot < S.Pigs.Num() * 3)
    {
        AHouseRoamingPig* Pig = S.Pigs[S.Shot / 3].Get();
        if (!Pig) { S.Safe = false; ++S.Shot; return; }
        FVector Target = Pig->FeetLocation() + FVector(0, 0, 40.f * Pig->VisualScale);
        TInlineComponentArray<UStaticMeshComponent*> Parts(Pig);
        for (UStaticMeshComponent* Part : Parts)
            if (Part && Part->GetFName()==TEXT("PigBody")) Target=Part->Bounds.Origin;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(CalderPigCamera), false, Pig);
        for (TActorIterator<APawn> It(World); It; ++It) Query.AddIgnoredActor(*It);
        bool Clear = false;
        const float Angle0 = S.Shot % 3 == 0 ? -70.f : (S.Shot % 3 == 1 ? 70.f : 180.f);
        for (const float Distance : {180.f,140.f,110.f,220.f})
        {
            for (const float Offset : {0.f,30.f,-30.f,60.f,-60.f,90.f,-90.f,120.f,-120.f,150.f,-150.f,180.f})
            {
                const FVector Eye = FVector(Target.X,Target.Y,Pig->FeetLocation().Z+170.f) + FRotator(0, Pig->GetActorRotation().Yaw + Angle0 + Offset, 0).Vector() * Distance;
                FHitResult Hit;
                if (World->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Query)) continue;
                S.Camera->SetActorLocationAndRotation(Eye, (Target - Eye).Rotation());
                Clear = true; break;
            }
            if (Clear) break;
        }
        S.Safe &= Clear;
        if (Clear)
        {
            if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
            const FString Capture = S.Dir / FString::Printf(TEXT("%s_%d.png"), *Pig->PigName, S.Shot % 3);
            IFileManager::Get().Delete(*Capture);
            FScreenshotRequest::RequestScreenshot(Capture, false, false);
            S.Captures.Add(Capture);
        }
        ++S.Shot; S.Next = Now + 1.5;
        return;
    }
    for (const FString& Capture : S.Captures) S.Safe &= IFileManager::Get().FileSize(*Capture) > 0;
    S.Safe &= S.Captures.Num() == 6;
    S.Done = true;
    FFileHelper::SaveStringToFile(S.Safe ? TEXT("PASS assembly/captures only; locomotion unverified\n") : TEXT("FAIL assembly/captures\n"), *(S.Dir / TEXT("result.txt")));
    UE_LOG(LogTemp, Display, TEXT("CALDER_PIG_VISUAL_REVIEW %s pigs=%d captures=%d physicalMotionNotCovered=1"), S.Safe ? TEXT("PASS") : TEXT("FAIL"), S.Pigs.Num(), S.Captures.Num());
    FPlatformMisc::RequestExitWithStatus(false, S.Safe ? 0 : 1);
}
struct FRegistration { FRegistration() { FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick); } };
FRegistration Registration;
}
#endif
