// Opt-in (-HouseStairSurvey): what is in the upstairs stairwell, how the player moves up it, and how it renders.
#include "HouseCarryTiersReview.h"
#include "StoryCampaign.h"
#include "HouseWorldScale.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderCore.h"
#include "PrimitiveSceneProxy.h"
#include "GroomComponent.h"
#include "RHI.h"
#include "StaticMeshResources.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"

namespace
{
struct FWalkCfg { const TCHAR* Name; const TCHAR* Cmd; int32 Special; };
const FWalkCfg WalkCfgs[] = {
    { TEXT("cold"), TEXT(""), 0 },
    { TEXT("warm"), TEXT(""), 0 },
    { TEXT("base_a"), TEXT(""), 0 },
};
const int32 NumWalk = UE_ARRAY_COUNT(WalkCfgs);
struct FWalkAcc { int32 N = 0; double Dt = 0, Game = 0, Gpu = 0, Max = 0; TArray<float> All; };
struct FStView { FString Name; FVector Pos, Target; };
struct FStState
{
    TWeakObjectPtr<UWorld> World; TWeakObjectPtr<ACameraActor> Cam;
    int32 Step = 0, Way = 0, View = 0, Shot = 0, Pass = 0, Cfg = 0;
    TArray<TPair<FString, FString>> SavedCVars; FWalkAcc Acc; TArray<TPair<TWeakObjectPtr<ULightComponent>, float>> LightDraw; TArray<TWeakObjectPtr<AActor>> Frozen;
    int32 Sweep = -1; TArray<TWeakObjectPtr<UPrimitiveComponent>> SweepList; TArray<TWeakObjectPtr<UPrimitiveComponent>> Hidden; TArray<TWeakObjectPtr<ULightComponent>> Unshadowed, WalkUnshadowed; double Started = 0, StepStarted = 0, LastShot = 0, LastProgress = 0;
    float BestDist = 1e9f; FString Dir; FString Csv; FVector LastPos = FVector::ZeroVector; FRotator Look = FRotator::ZeroRotator;
    TArray<FVector> Path; TArray<FStView> Views;
    TMap<TWeakObjectPtr<UPrimitiveComponent>, TPair<FPrimitiveSceneProxy*, FTransform>> Track; TMap<FString, int32> Recreated, Moved; TArray<TWeakObjectPtr<UPrimitiveComponent>> BodyTouched;
};

FBox StBox(const UWorld* World)
{
    return HouseWorldScale::Bounds(World,
        FBox(FVector(-2360, 840, -40), FVector(-1400, 1300, 640)));
}

void StCensus(UWorld* World, const FString& Dir)
{
    FString Out;
    int32 Prims = 0, Tris = 0;
    TMap<FString, int32> Dupes;
    for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
    {
        UPrimitiveComponent* C = *It;
        if (!IsValid(C) || C->GetWorld() != World || !C->IsRegistered()) continue;
        const FBox B = C->Bounds.GetBox();
        if (!B.Intersect(StBox(World))) continue;
        if (B.GetSize().GetMax() > 4000.f) continue;   // skip whole-house tiles, listed separately below
        ++Prims;
        FString Mesh, Mats; int32 T = 0; FString Blend;
        if (const UStaticMeshComponent* S = Cast<UStaticMeshComponent>(C))
        {
            if (UStaticMesh* M = S->GetStaticMesh())
            {
                Mesh = M->GetName();
                if (M->GetRenderData() && M->GetRenderData()->LODResources.Num()) T = M->GetRenderData()->LODResources[0].GetNumTriangles();
                if (const UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(S)) T *= FMath::Max(1, I->GetInstanceCount());
            }
        }
        for (int32 K = 0; K < C->GetNumMaterials(); ++K)
            if (UMaterialInterface* Mi = C->GetMaterial(K))
            {
                Mats += Mi->GetName() + TEXT(",");
                const EBlendMode BM = Mi->GetBlendMode();
                if (BM != BLEND_Opaque && BM != BLEND_Masked) Blend += TEXT("TRANSLUCENT ");
            }
        Tris += T;
        const FString Key = Mesh + FString::Printf(TEXT("@%.0f,%.0f,%.0f"), C->GetComponentLocation().X, C->GetComponentLocation().Y, C->GetComponentLocation().Z);
        if (!Mesh.IsEmpty()) Dupes.FindOrAdd(Key)++;
        Out += FString::Printf(TEXT("%s | owner=%s(%s) | mesh=%s tris=%d | vis=%d hidden=%d shadow=%d mob=%d col=%d | %s | ext=%s | loc=%s | mats=%s\n"),
            *C->GetName(), *GetNameSafe(C->GetOwner()), C->GetOwner() ? *C->GetOwner()->GetClass()->GetName() : TEXT("-"), *Mesh, T,
            C->IsVisible() ? 1 : 0, C->bHiddenInGame ? 1 : 0, C->CastShadow ? 1 : 0, (int32)C->Mobility, (int32)C->GetCollisionEnabled(), *Blend,
            *B.GetExtent().ToCompactString(), *B.GetCenter().ToCompactString(), *Mats.Left(160));
    }
    Out += TEXT("---- duplicates (same mesh, same location)\n");
    for (const auto& P : Dupes) if (P.Value > 1) Out += FString::Printf(TEXT("DUP x%d %s\n"), P.Value, *P.Key);
    Out += TEXT("---- lights reaching the stairwell\n");
    int32 Lights = 0;
    for (TObjectIterator<ULightComponent> It; It; ++It)
    {
        ULightComponent* L = *It;
        if (!IsValid(L) || L->GetWorld() != World || !L->IsRegistered()) continue;
        const FVector P = L->GetComponentLocation();
        const float R = L->GetBoundingSphere().W;
        if (StBox(World).ComputeSquaredDistanceToPoint(P) > R * R) continue;
        ++Lights;
        Out += FString::Printf(TEXT("LIGHT %s owner=%s cls=%s loc=%s r=%.0f int=%.1f shadow=%d vis=%d draw=%.0f\n"), *L->GetName(), *GetNameSafe(L->GetOwner()),
            *L->GetClass()->GetName(), *P.ToCompactString(), R, L->Intensity, L->CastShadows ? 1 : 0, L->IsVisible() ? 1 : 0, L->MaxDrawDistance);
    }
    FFileHelper::SaveStringToFile(Out, *FPaths::Combine(Dir, TEXT("stair_census.txt")));
    UE_LOG(LogTemp, Display, TEXT("HOUSE_STAIR_CENSUS prims=%d tris=%d lights=%d"), Prims, Tris, Lights);
    {
        TMap<FString, FIntVector4> ByOwner; int32 Proxies = 0, HiddenProxies = 0, NoShadowCull = 0;
        for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
        {
            UPrimitiveComponent* C = *It;
            if (!IsValid(C) || C->GetWorld() != World || !C->IsRegistered() || !C->SceneProxy) continue;
            ++Proxies;
            const bool bHid = C->bHiddenInGame || !C->IsVisible();
            if (bHid) ++HiddenProxies;
            if (C->CastShadow) ++NoShadowCull;
            FIntVector4& V = ByOwner.FindOrAdd(C->GetOwner() ? C->GetOwner()->GetClass()->GetName() : TEXT("-"));
            V.X++; if (bHid) V.Y++; if (C->CastShadow) V.Z++; if (C->CachedMaxDrawDistance <= 0.f && C->LDMaxDrawDistance <= 0.f) V.W++;
        }
        ByOwner.ValueSort([](const FIntVector4& A, const FIntVector4& B) { return A.X > B.X; });
        UE_LOG(LogTemp, Display, TEXT("HOUSE_STAIR_PROXIES total=%d hidden=%d shadow=%d"), Proxies, HiddenProxies, NoShadowCull);
        int32 Shown = 0;
        for (const auto& P : ByOwner) { if (Shown++ >= 25) break; UE_LOG(LogTemp, Display, TEXT("HOUSE_STAIR_PROXIES %s n=%d hidden=%d shadow=%d nodrawdist=%d"), *P.Key, P.Value.X, P.Value.Y, P.Value.Z, P.Value.W); }
    }
    for (const TCHAR* N : { TEXT("sg.ShadowQuality"), TEXT("sg.GlobalIlluminationQuality"), TEXT("sg.PostProcessQuality"), TEXT("r.MegaLights.Allow"), TEXT("r.MegaLights.EnableForProject"), TEXT("r.Shadow.Virtual.SMRT.RayCountLocal"), TEXT("r.ShadowQuality") })
        if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(N)) UE_LOG(LogTemp, Display, TEXT("HOUSE_STAIR_CVAR %s=%s"), N, *V->GetString());
}

float StGpu() { return FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()); }


}

void StApplyCmds(UWorld* World, const TCHAR* Cmds, TArray<TPair<FString, FString>>& Saved)
{
    TArray<FString> Parts; FString(Cmds).ParseIntoArray(Parts, TEXT(";"), true);
    for (FString& Cmd : Parts)
    {
        FString Name, Value; if (!Cmd.Split(TEXT(" "), &Name, &Value)) continue;
        if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(*Name)) Saved.Add({ Name, V->GetString() });
        GEngine->Exec(World, *Cmd);
    }
}
void StWalkBegin(FStState& S, UWorld* World)
{
    const FWalkCfg& C = WalkCfgs[S.Pass];
    S.Acc = FWalkAcc();
    if (C.Cmd[0]) StApplyCmds(World, C.Cmd, S.SavedCVars);
    if (C.Special == 6) GEngine->Exec(World, TEXT("ProfileGPU"));
    if (C.Special == 1)
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            const FString Cls = It->GetClass()->GetName();
            if (!Cls.Contains(TEXT("InteractiveProp")) && !Cls.Contains(TEXT("KitProp"))) continue;
            if (StBox(World).ComputeSquaredDistanceToPoint(It->GetActorLocation()) > 600.f * 600.f) continue;
            It->SetActorTickEnabled(false); It->SetActorEnableCollision(false); S.Frozen.Add(*It);
        }
    if (C.Special == 2)
        for (TObjectIterator<ULightComponent> It; It; ++It)
        {
            ULightComponent* L = *It;
            if (!IsValid(L) || L->GetWorld() != World || L->IsA<UDirectionalLightComponent>()) continue;
            S.LightDraw.Add({ L, L->MaxDrawDistance }); L->MaxDrawDistance = 1200.f; L->MarkRenderStateDirty();
        }
    if (C.Special == 14)
        for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
        {
            UPrimitiveComponent* P = *It;
            if (!IsValid(P) || P->GetWorld() != World || !P->CastShadow || !P->GetOwner() || P->Bounds.SphereRadius > 40.f) continue;
            const FString Cls = P->GetOwner()->GetClass()->GetName();
            if (!Cls.Contains(TEXT("InteractiveProp")) && !Cls.Contains(TEXT("KitProp")) && !Cls.Contains(TEXT("Litter")) && !Cls.Contains(TEXT("ReactiveFixture"))) continue;
            P->SetCastShadow(false); S.BodyTouched.Add(P);
        }
    if (C.Special == 15 || C.Special == 16)
        for (TObjectIterator<ULightComponent> It; It; ++It)
        {
            ULightComponent* L = *It;
            if (!IsValid(L) || L->GetWorld() != World || L->IsA<UDirectionalLightComponent>()) continue;
            S.LightDraw.Add({ L, L->MaxDrawDistance }); L->MaxDrawDistance = C.Special == 16 ? 3200.f : 1800.f; L->MaxDistanceFadeRange = L->MaxDrawDistance * .2f; L->MarkRenderStateDirty();
        }
    if (C.Special == 12 || C.Special == 13)
        for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
        {
            UPrimitiveComponent* P = *It;
            if (!IsValid(P) || P->GetWorld() != World || !P->IsVisible() || !P->GetOwner()) continue;
            const FString Cls = P->GetOwner()->GetClass()->GetName();
            if (C.Special == 12 && !Cls.Contains(TEXT("InteractiveProp")) && !Cls.Contains(TEXT("Litter")) && !Cls.Contains(TEXT("ReactiveFixture"))) continue;
            if (C.Special == 13 && (Cls.Contains(TEXT("Character")) || Cls.Contains(TEXT("Braxton")) || P->Bounds.SphereRadius > 2000.f)) continue;
            if (StBox(World).ComputeSquaredDistanceToPoint(P->Bounds.Origin) < 1200.f * 1200.f) continue;
            P->SetVisibility(false); S.BodyTouched.Add(P);
        }
    if (C.Special >= 8 && C.Special <= 11)
        for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
        {
            UPrimitiveComponent* P = *It;
            if (!IsValid(P) || P->GetWorld() != World || !P->GetOwner() || !P->GetOwner()->GetClass()->GetName().Contains(TEXT("Braxton"))) continue;
            UGroomComponent* G = Cast<UGroomComponent>(P);
            if (C.Special == 8 && G) P->SetVisibility(false);
            else if (C.Special == 9 && G) G->SetUseCards(true);
            else if (C.Special == 10) P->SetCastShadow(false);
            else if (C.Special == 11) P->SetVisibility(false);
            else continue;
            S.BodyTouched.Add(P);
        }
    if (C.Special == 3 || C.Special == 4 || C.Special == 5)
        for (TObjectIterator<ULightComponent> It; It; ++It)
        {
            ULightComponent* L = *It;
            if (!IsValid(L) || L->GetWorld() != World || !L->CastShadows) continue;
            if (C.Special != 5 && L->IsA<UDirectionalLightComponent>() != (C.Special == 3)) continue;
            L->SetCastShadows(false); S.WalkUnshadowed.Add(L);
        }
}
void StWalkEnd(FStState& S, UWorld* World)
{
    const FWalkAcc& A = S.Acc;
    if (A.N)
    {
        TArray<float> Sorted = A.All; Sorted.Sort();
        UE_LOG(LogTemp, Display, TEXT("HOUSE_STAIR_WALK %s frames=%d dt=%.1f med=%.1f p90=%.1f game=%.1f gpu=%.1f max=%.1f"), WalkCfgs[S.Pass].Name, A.N, A.Dt / A.N,
            Sorted[Sorted.Num() / 2], Sorted[FMath::Min(Sorted.Num() - 1, Sorted.Num() * 9 / 10)], A.Game / A.N, A.Gpu / A.N, A.Max);
    }
    for (const TPair<FString, FString>& Prev : S.SavedCVars) GEngine->Exec(World, *(Prev.Key + TEXT(" ") + Prev.Value));
    S.SavedCVars.Reset();
    for (const TWeakObjectPtr<AActor>& F : S.Frozen) if (F.IsValid()) { F->SetActorTickEnabled(true); F->SetActorEnableCollision(true); }
    S.Frozen.Reset();
    for (const auto& P : S.LightDraw) if (P.Key.IsValid()) { P.Key->MaxDrawDistance = P.Value; P.Key->MarkRenderStateDirty(); }
    S.LightDraw.Reset();
    for (const TWeakObjectPtr<ULightComponent>& L : S.WalkUnshadowed) if (L.IsValid()) L->SetCastShadows(true);
    S.WalkUnshadowed.Reset();
    for (const TWeakObjectPtr<UPrimitiveComponent>& W : S.BodyTouched)
        if (UPrimitiveComponent* P = W.Get())
        {
            const int32 Sp = WalkCfgs[S.Pass].Special;
            if (Sp == 8 || Sp == 11 || Sp == 12 || Sp == 13) P->SetVisibility(true);
            if (Sp == 9) if (UGroomComponent* G = Cast<UGroomComponent>(P)) G->SetUseCards(false);
            if (Sp == 10 || Sp == 14) P->SetCastShadow(true);
        }
    S.BodyTouched.Reset();
    if (WalkCfgs[S.Pass].Special == 7)
    {
        for (int32 Kind = 0; Kind < 2; ++Kind)
        {
            TMap<FString, int32>& M = Kind ? S.Moved : S.Recreated;
            M.ValueSort([](int32 A, int32 B) { return A > B; });
            int32 Shown = 0, Total = 0;
            for (const auto& P : M) Total += P.Value;
            UE_LOG(LogTemp, Display, TEXT("HOUSE_STAIR_%s total=%d comps=%d frames=%d"), Kind ? TEXT("MOVED") : TEXT("RECREATED"), Total, M.Num(), A.N);
            for (const auto& P : M) { if (Shown++ >= 30) break; UE_LOG(LogTemp, Display, TEXT("HOUSE_STAIR_%s %d %s"), Kind ? TEXT("MOVED") : TEXT("RECREATED"), P.Value, *P.Key); }
        }
        S.Track.Reset(); S.Recreated.Reset(); S.Moved.Reset();
    }
}

void TickHouseStairSurvey(AStoryFirstPersonCharacter* Pawn)
{
    if (!Pawn || !Pawn->HasAuthority() || !Pawn->IsLocallyControlled()) return;
    UWorld* World = Pawn->GetWorld();
    if (!World || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
    if (!PC) return;
    static FStState S;
    const double Now = World->GetTimeSeconds();
    if (S.World.Get() != World)
    {
        S = FStState(); S.World = World; S.Started = S.StepStarted = Now;
        S.Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HouseStairSurvey")));
        IFileManager::Get().MakeDirectory(*S.Dir, true);
        // Ground floor -> north flight up to the window landing -> south flight up to the second floor.
        S.Path = { FVector(-2150, 1130, 1), FVector(-2020, 1130, 1), FVector(-1640, 1130, 150), FVector(-1500, 1110, 169), FVector(-1500, 950, 169),
                   FVector(-1600, 940, 169), FVector(-2010, 940, 305), FVector(-2200, 990, 305), FVector(-2195, 700, 300) };
        if (FParse::Param(FCommandLine::Get(), TEXT("StairWallWalk")))
        {
            // Verify the repaired wall leaves both flights clear in both directions.
            S.Path.SetNum(8);
            for (int32 I = 6; I >= 0; --I) { const FVector Back = S.Path[I]; S.Path.Add(Back); }
            const FVector Start = S.Path[0]; S.Path.Add(Start); // Existing path walker reserves the final point.
        }
        for (FVector& Point : S.Path) Point = HouseWorldScale::Point(World, Point);
        S.Views = {
            { TEXT("v01_bottom_up"), FVector(-2200, 1130, 160), FVector(-1600, 1130, 260) },
            { TEXT("v02_flightA_mid"), FVector(-1800, 1130, 240), FVector(-1500, 1050, 300) },
            { TEXT("v03_landing_window"), FVector(-1620, 1000, 330), FVector(-1430, 1050, 300) },
            { TEXT("v04_landing_back"), FVector(-1480, 1050, 330), FVector(-2000, 950, 400) },
            { TEXT("v05_flightB_up"), FVector(-1560, 940, 330), FVector(-2100, 940, 450) },
            { TEXT("v06_flightB_down"), FVector(-2040, 940, 470), FVector(-1500, 1000, 300) },
            { TEXT("v07_top_landing"), FVector(-2000, 1000, 470), FVector(-2290, 1000, 380) },
            { TEXT("v08_top_down_well"), FVector(-2100, 1050, 480), FVector(-1700, 1100, 100) },
            { TEXT("v09_ceiling_up"), FVector(-1800, 1050, 350), FVector(-1790, 1060, 700) },
            { TEXT("v10_great_room"), FVector(1076, 567, 175), FVector(76, 267, 150) },
            { TEXT("v11_long_hall"), FVector(-2212, -258, 170), FVector(-2212, 742, 165) },
            { TEXT("v12_great_room_back"), FVector(-300, 100, 175), FVector(900, 500, 150) },
        };
        for (FStView& View : S.Views)
        {
            View.Pos = HouseWorldScale::Point(World, View.Pos);
            View.Target = HouseWorldScale::Point(World, View.Target);
        }
        S.Csv = TEXT("pass,t,way,x,y,z,vx,vy,vz,mode,floorz,dt_ms,game_ms,render_ms,gpu_ms\n");
    }
    if (S.Step < 0) return;
    const double Age = Now - S.StepStarted;
    UCharacterMovementComponent* Move = Pawn->GetCharacterMovement();
    const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (S.Step == 0)
    {
        if (Age < 6.0) return;
        StCensus(World, S.Dir);
        if (PC->MyHUD) PC->MyHUD->bShowHUD = false;
        Pawn->SetActorLocation(S.Path[0] + FVector(0, 0, Half + 4.f), false, nullptr, ETeleportType::TeleportPhysics);
        Move->SetMovementMode(MOVE_Walking);
        S.Way = 1; S.Step = 1; S.StepStarted = Now; S.LastProgress = Now; S.BestDist = 1e9f; S.LastPos = Pawn->GetActorLocation();
        S.Pass = 0; StWalkBegin(S, World);
        return;
    }
    if (S.Step == 1)
    {
        // Walk the path at normal speed, logging every frame.
        const FVector Here = Pawn->GetActorLocation();
        const FVector Goal = S.Path[S.Way];
        FVector To = Goal - (Here - FVector(0, 0, Half));
        const float Dist2D = FVector(To.X, To.Y, 0).Size();
        if (Age > 1.0)
        {
            const FFindFloorResult& F = Move->CurrentFloor;
            const float Dt = FApp::GetDeltaTime() * 1000.f;
            if (WalkCfgs[S.Pass].Special == 7)
                for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
                {
                    UPrimitiveComponent* C = *It;
                    if (!IsValid(C) || C->GetWorld() != World || !C->IsRegistered()) continue;
                    const FString Key = FString::Printf(TEXT("%s.%s(%s)"), C->GetOwner() ? *C->GetOwner()->GetClass()->GetName() : TEXT("-"), *C->GetName(), *C->GetClass()->GetName());
                    if (auto* Prev = S.Track.Find(C))
                    {
                        if (Prev->Key != C->SceneProxy) S.Recreated.FindOrAdd(Key)++;
                        if (!Prev->Value.Equals(C->GetComponentTransform(), 0.01f)) S.Moved.FindOrAdd(Key)++;
                    }
                    S.Track.Add(C, TPair<FPrimitiveSceneProxy*, FTransform>(C->SceneProxy, C->GetComponentTransform()));
                }
            S.Acc.N++; S.Acc.All.Add(Dt); S.Acc.Dt += Dt; S.Acc.Game += FPlatformTime::ToMilliseconds(GGameThreadTime); S.Acc.Gpu += StGpu(); S.Acc.Max = FMath::Max<double>(S.Acc.Max, Dt);
            S.Csv += FString::Printf(TEXT("%d,%.3f,%d,%.1f,%.1f,%.1f,%.0f,%.0f,%.0f,%d,%.1f,%.2f,%.2f,%.2f,%.2f\n"), S.Pass, Now - S.StepStarted, S.Way, Here.X, Here.Y, Here.Z - Half,
                Pawn->GetVelocity().X, Pawn->GetVelocity().Y, Pawn->GetVelocity().Z, (int32)Move->MovementMode, F.bBlockingHit ? F.HitResult.ImpactPoint.Z : -9999.f,
                FApp::GetDeltaTime() * 1000.f, FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), StGpu());
        }
        if (Dist2D < 25.f)
        {
            if (++S.Way >= S.Path.Num() - 1)
            {
                // Two passes: the first visit (cold) and a second, warm one without screenshots.
                StWalkEnd(S, World);
                if (FParse::Param(FCommandLine::Get(), TEXT("StairWallWalk")))
                {
                    FFileHelper::SaveStringToFile(S.Csv, *FPaths::Combine(S.Dir, TEXT("wall_clearance.csv")));
                    UE_LOG(LogTemp, Display, TEXT("STAIR_WALL_WALK_PASS both flights up and down"));
                    S.Step = -1; FPlatformMisc::RequestExitWithStatus(false, 0); return;
                }
                if (++S.Pass < NumWalk)
                {
                    StWalkBegin(S, World);
                    Pawn->SetActorLocation(S.Path[0] + FVector(0, 0, Half + 4.f), false, nullptr, ETeleportType::TeleportPhysics);
                    S.Way = 1; S.StepStarted = Now; S.LastProgress = Now; S.BestDist = 1e9f; S.Look = FRotator::ZeroRotator;
                    return;
                }
                S.Step = 2; S.StepStarted = Now; S.View = 0; S.Shot = 0; return;
            }
            S.BestDist = 1e9f; S.LastProgress = Now;
            return;
        }
        if (Dist2D < S.BestDist - 5.f) { S.BestDist = Dist2D; S.LastProgress = Now; }
        if (Now - S.LastProgress > 4.0)
        {
            UE_LOG(LogTemp, Display, TEXT("HOUSE_STAIR_STUCK way=%d at %s goal=%s"), S.Way, *Here.ToCompactString(), *Goal.ToCompactString());
            S.Csv += FString::Printf(TEXT("# STUCK way=%d\n"), S.Way);
            if (FParse::Param(FCommandLine::Get(), TEXT("StairWallWalk")))
            {
                FFileHelper::SaveStringToFile(S.Csv, *FPaths::Combine(S.Dir, TEXT("wall_clearance.csv")));
                UE_LOG(LogTemp, Error, TEXT("STAIR_WALL_WALK_FAIL way=%d"), S.Way);
                S.Step = -1; FPlatformMisc::RequestExitWithStatus(false, 1); return;
            }
            Pawn->SetActorLocation(Goal + FVector(0, 0, Half + 4.f), false, nullptr, ETeleportType::TeleportPhysics);
            S.LastProgress = Now; S.BestDist = 1e9f;
            return;
        }
        To.Z = 0.f;
        const FVector D = To.GetSafeNormal();
        Pawn->AddMovementInput(D, 1.f);
        const FRotator Want = (D + FVector(0, 0, Goal.Z > Here.Z - Half + 20.f ? .25f : -.2f)).Rotation();
        S.Look = FMath::RInterpTo(S.Look.IsZero() ? Want : S.Look, Want, FApp::GetDeltaTime(), 4.f);
        PC->SetControlRotation(S.Look);
        if (S.Pass == 0 && Now - S.LastShot > 1.2) { S.LastShot = Now; FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir, FString::Printf(TEXT("walk_%02d.png"), S.Shot++)), false, false); }
        if (Age > 60.0) { StWalkEnd(S, World); S.Step = 2; S.StepStarted = Now; S.View = 0; S.Shot = 0; }
        return;
    }
    if (S.Step == 2)
    {
        // Flicker matrix: for each suspect, three views, four frames 0.2 s apart (compared offline).
        struct FCfg { const TCHAR* Name; const TCHAR* Cmd; int32 Hide; };
        static const FCfg Cfgs[] = {
            { TEXT("base"), TEXT(""), 0 },
            { TEXT("lights_nolimit"), TEXT(""), 9 },
            { TEXT("nanite_off"), TEXT("r.Nanite 0"), 0 },
        };
        static const int32 FlViews[] = { 9, 11, 7 };   // v08 (the flickering view), bottom of the stairs, big rooms
        const int32 NumCfg = UE_ARRAY_COUNT(Cfgs), NumV = UE_ARRAY_COUNT(FlViews);
        if (!S.Cam.IsValid())
        {
            FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ACameraActor* C = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, P);
            if (C && Pawn->FirstPersonCamera) C->GetCameraComponent()->PostProcessSettings = Pawn->FirstPersonCamera->PostProcessSettings;
            if (C) { C->GetCameraComponent()->SetFieldOfView(80.f); C->GetCameraComponent()->bConstrainAspectRatio = false; }
            S.Cam = C;
            Pawn->SetActorHiddenInGame(true);
            Pawn->SetActorLocation(FVector(0, 0, 10000), false, nullptr, ETeleportType::TeleportPhysics);
            Move->SetMovementMode(MOVE_None);
            S.Cfg = -1; S.View = NumV; S.Shot = 0; S.StepStarted = Now; return;
        }
        if (FScreenshotRequest::IsScreenshotRequested()) return;
        if (S.View >= NumV)
        {
            // Undo the previous config, apply the next.
            for (const TPair<FString, FString>& Prev : S.SavedCVars) GEngine->Exec(World, *(Prev.Key + TEXT(" ") + Prev.Value));
            S.SavedCVars.Reset();
            for (const TWeakObjectPtr<UPrimitiveComponent>& H : S.Hidden) if (H.IsValid()) H->SetVisibility(true);
            S.Hidden.Reset();
            for (const TWeakObjectPtr<ULightComponent>& L : S.Unshadowed) if (L.IsValid()) L->SetCastShadows(true);
            S.Unshadowed.Reset();
            for (const auto& P : S.LightDraw) if (P.Key.IsValid()) { P.Key->MaxDrawDistance = P.Value; P.Key->MarkRenderStateDirty(); }
            S.LightDraw.Reset();
            if (S.Cfg >= 0 && S.Sweep >= 0)
            {
                // Component sweep: restore the last hidden one, hide the next.
                if (S.Sweep < S.SweepList.Num() && S.SweepList[S.Sweep].IsValid()) S.SweepList[S.Sweep]->SetVisibility(true);
                if (++S.Sweep < S.SweepList.Num())
                {
                    UPrimitiveComponent* H = S.SweepList[S.Sweep].Get();
                    if (H) H->SetVisibility(false);
                    UE_LOG(LogTemp, Display, TEXT("HOUSE_STAIR_SWEEP %d %s owner=%s mesh=%s"), S.Sweep, *GetNameSafe(H), H ? *GetNameSafe(H->GetOwner()) : TEXT("-"),
                        H && Cast<UStaticMeshComponent>(H) ? *GetNameSafe(Cast<UStaticMeshComponent>(H)->GetStaticMesh()) : TEXT("-"));
                    S.View = 0; S.Shot = 0; S.StepStarted = Now; return;
                }
                S.Sweep = -2;
            }
            if (S.Cfg == 0 && S.Sweep == -1 && FParse::Param(FCommandLine::Get(), TEXT("StairSweep")))
            {
                // After the base config: sweep every visible primitive near the flights.
                const FBox Flights = HouseWorldScale::Bounds(World,
                    FBox(FVector(-2120, 840, -20), FVector(-1400, 1300, 330)));
                for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
                {
                    UPrimitiveComponent* P = *It;
                    if (!IsValid(P) || P->GetWorld() != World || !P->IsRegistered() || !P->IsVisible() || P->bHiddenInGame) continue;
                    if (!P->Bounds.GetBox().Intersect(Flights)) continue;
                    if (P->GetOwner() == Pawn || P->GetOwner() == S.Cam.Get()) continue;
                    S.SweepList.Add(P);
                }
                UE_LOG(LogTemp, Display, TEXT("HOUSE_STAIR_SWEEP_LIST %d"), S.SweepList.Num());
                S.Sweep = 0;
                if (S.SweepList.Num())
                {
                    UPrimitiveComponent* H = S.SweepList[0].Get(); H->SetVisibility(false);
                    UE_LOG(LogTemp, Display, TEXT("HOUSE_STAIR_SWEEP 0 %s owner=%s mesh=%s"), *GetNameSafe(H), *GetNameSafe(H->GetOwner()),
                        Cast<UStaticMeshComponent>(H) ? *GetNameSafe(Cast<UStaticMeshComponent>(H)->GetStaticMesh()) : TEXT("-"));
                    S.View = 0; S.Shot = 0; S.StepStarted = Now; return;
                }
            }
            if (++S.Cfg >= NumCfg)
            {
                FFileHelper::SaveStringToFile(S.Csv, *FPaths::Combine(S.Dir, TEXT("stair_walk.csv")));
                UE_LOG(LogTemp, Display, TEXT("HOUSE_STAIR_SURVEY_DONE seconds=%.0f"), Now - S.Started);
                S.Step = -1; FPlatformMisc::RequestExitWithStatus(false, 0); return;
            }
            const FCfg& Cf = Cfgs[S.Cfg];
            if (Cf.Cmd[0])
            {
                FString Name, Value; FString(Cf.Cmd).Split(TEXT(" "), &Name, &Value);
                if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(*Name)) S.SavedCVars.Add({ Name, V->GetString() });
                GEngine->Exec(World, Cf.Cmd);
            }
            if (Cf.Hide == 1)
                for (TObjectIterator<ULightComponent> It; It; ++It)
                {
                    ULightComponent* L = *It;
                    if (!IsValid(L) || L->GetWorld() != World || !L->CastShadows || L->IsA<UDirectionalLightComponent>()) continue;
                    const float R = L->GetBoundingSphere().W;
                    if (StBox(World).ComputeSquaredDistanceToPoint(L->GetComponentLocation()) > R * R) continue;
                    L->SetCastShadows(false); S.Unshadowed.Add(L);
                }
            if (Cf.Hide == 9)
                for (TObjectIterator<ULightComponent> It; It; ++It)
                {
                    ULightComponent* L = *It;
                    if (!IsValid(L) || L->GetWorld() != World || L->IsA<UDirectionalLightComponent>()) continue;
                    S.LightDraw.Add({ L, L->MaxDrawDistance }); L->MaxDrawDistance = 0.f; L->MarkRenderStateDirty();
                }
            if (Cf.Hide >= 2 && Cf.Hide <= 3)
            {
                const TCHAR* Key = Cf.Hide == 2 ? TEXT("SM_Upper_p0_m2") : TEXT("SM_Entrance_p0_m2");
                for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
                {
                    UStaticMeshComponent* M = *It;
                    if (!IsValid(M) || M->GetWorld() != World || !M->GetStaticMesh() || !M->IsVisible()) continue;
                    if (!M->GetStaticMesh()->GetName().Contains(Key) || M->GetStaticMesh()->GetName().StartsWith(TEXT("COL"))) continue;
                    M->SetVisibility(false); S.Hidden.Add(M);
                    UE_LOG(LogTemp, Display, TEXT("HOUSE_STAIR_HIDE %s"), *M->GetStaticMesh()->GetName());
                }
            }
            S.View = 0; S.Shot = 0; S.StepStarted = Now; return;
        }
        const FStView& V = S.Views[FlViews[S.View]];
        ACameraActor* C = S.Cam.Get();
        C->SetActorLocationAndRotation(V.Pos, (V.Target - V.Pos).Rotation());
        if (PC->GetViewTarget() != C) PC->SetViewTarget(C);
        const double Wait = S.Shot == 0 ? 1.4 : .2;
        if (Age < Wait) return;
        const FString Tag = S.Sweep >= 0 ? FString::Printf(TEXT("sweep%02d"), S.Sweep) : FString(Cfgs[S.Cfg].Name);
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir, FString::Printf(TEXT("fl_%s_%s_%d.png"), *V.Name, *Tag, S.Shot)), false, false);
        S.StepStarted = Now;
        if (++S.Shot >= 4) { S.Shot = 0; ++S.View; }
    }
}
