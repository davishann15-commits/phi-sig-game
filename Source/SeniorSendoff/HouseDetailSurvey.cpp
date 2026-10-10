// Opt-in (-HouseDetailSurvey) whole-house census, frame-time probe and registered-view screenshot sweep.
#include "HouseCarryTiersReview.h"

#if !UE_BUILD_SHIPPING
#include "ExistingHouseDoor.h"
#include "HouseInteractiveProp.h"
#include "HouseReactiveFixture.h"
#include "StoryCampaign.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Engine/Engine.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "RenderCore.h"
#include "RHI.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
struct FSurveyProbe { FString Name; FVector Feet; FVector Dir; };
struct FSurveyView { FString Name; FVector Pos, Target; float Fov; };
struct FSurveyFrame { float Dt, Game, Render, Gpu; };
struct FHouseDetailSurveyState
{
    TWeakObjectPtr<UWorld> World; TWeakObjectPtr<ACameraActor> Cam;
    int32 Step = 0, Index = 0, Placed = -1; double Started = 0, StepStarted = 0; FString Dir;
    TArray<FSurveyProbe> Probes; TArray<FSurveyView> Views;
    TArray<FSurveyFrame> Frames; TArray<TSharedPtr<FJsonValue>> ProbeRows, MatrixRows;
    TArray<TPair<FString, FString>> SavedCVars;
    bool bWalking = false;
    int32 Config = 0, Spot = 0; TArray<TWeakObjectPtr<ULightComponent>> TouchedLights; TArray<bool> TouchedShadow, TouchedVisible; TArray<float> TouchedDraw;
};
struct FSurveyConfig { const TCHAR* Name; const TCHAR* Apply; const TCHAR* Restore; int32 LightMode; };
const FSurveyConfig SurveyConfigs[] = {
    { TEXT("baseline"), TEXT(""), TEXT(""), 0 },
    { TEXT("raytracing_off"), TEXT("r.RayTracing.Enable 0"), TEXT("r.RayTracing.Enable 1"), 0 },
    { TEXT("vsm_off"), TEXT("r.Shadow.Virtual.Enable 0"), TEXT("r.Shadow.Virtual.Enable 1"), 0 },
    { TEXT("no_shadow_lights"), TEXT(""), TEXT(""), 1 },
    { TEXT("only_shadow_lights"), TEXT(""), TEXT(""), 2 },
    { TEXT("lumen_reflections_off"), TEXT("r.Lumen.Reflections.Allow 0"), TEXT("r.Lumen.Reflections.Allow 1"), 0 },
    { TEXT("lumen_gi_off"), TEXT("r.Lumen.DiffuseIndirect.Allow 0"), TEXT("r.Lumen.DiffuseIndirect.Allow 1"), 0 },
    { TEXT("gi_quality_low"), TEXT("sg.GlobalIlluminationQuality 0"), TEXT("sg.GlobalIlluminationQuality 1"), 0 },
    { TEXT("shadow_quality_low"), TEXT("sg.ShadowQuality 0"), TEXT("sg.ShadowQuality 1"), 0 },
    { TEXT("screen_67"), TEXT("sg.ResolutionQuality 67"), TEXT("sg.ResolutionQuality 80"), 0 },
    { TEXT("screen_100"), TEXT("sg.ResolutionQuality 100"), TEXT("sg.ResolutionQuality 80"), 0 },
    { TEXT("lumen_probe_lowres"), TEXT("r.Lumen.ScreenProbeGather.DownsampleFactor 32"), TEXT("r.Lumen.ScreenProbeGather.DownsampleFactor 16"), 0 },
    { TEXT("ssr_instead_of_lumen_refl"), TEXT("r.ReflectionMethod 2"), TEXT("r.ReflectionMethod 1"), 0 },
    { TEXT("lights_uncull"), TEXT(""), TEXT(""), 5 },
    { TEXT("lights_cull_2000"), TEXT(""), TEXT(""), 4 },
    { TEXT("vsm_local_bias0"), TEXT("r.Shadow.Virtual.ResolutionLodBiasLocal 0"), TEXT("r.Shadow.Virtual.ResolutionLodBiasLocal 1"), 0 },
    { TEXT("vsm_local_bias2"), TEXT("r.Shadow.Virtual.ResolutionLodBiasLocal 2"), TEXT("r.Shadow.Virtual.ResolutionLodBiasLocal 1"), 0 },
    // "x_" configs: cheap shadow / Lumen trims (-MatrixSet=x runs only these plus the baseline; previous values are restored automatically).
    { TEXT("x_smrt_rays4"), TEXT("r.Shadow.Virtual.SMRT.RayCountLocal 4"), TEXT(""), 0 },
    { TEXT("x_smrt_samples4"), TEXT("r.Shadow.Virtual.SMRT.SamplesPerRayLocal 4"), TEXT(""), 0 },
    { TEXT("x_smrt_rays2_samples4"), TEXT("r.Shadow.Virtual.SMRT.RayCountLocal 2;r.Shadow.Virtual.SMRT.SamplesPerRayLocal 4"), TEXT(""), 0 },
    { TEXT("x_vsm_pages2048"), TEXT("r.Shadow.Virtual.MaxPhysicalPages 2048"), TEXT(""), 0 },
    { TEXT("x_vsm_coarse0"), TEXT("r.Shadow.Virtual.NonNanite.IncludeInCoarsePages 0"), TEXT(""), 0 },
    { TEXT("x_lumen_octa4"), TEXT("r.Lumen.ScreenProbeGather.TracingOctahedronResolution 4"), TEXT(""), 0 },
    { TEXT("x_lumen_refl_down2"), TEXT("r.Lumen.Reflections.DownsampleFactor 2"), TEXT(""), 0 },
    { TEXT("x_lumen_screentraces0"), TEXT("r.Lumen.ScreenProbeGather.ScreenTraces 0"), TEXT(""), 0 },
    { TEXT("x_shadowcull_off"), TEXT("house.ShadowCullDistance 0"), TEXT(""), 0 },
    { TEXT("x_shadowcull_1500"), TEXT("house.ShadowCullDistance 1500"), TEXT(""), 0 },
    { TEXT("x_shadowcull_1000"), TEXT("house.ShadowCullDistance 1000"), TEXT(""), 0 },
    { TEXT("x_combo"), TEXT("r.Shadow.Virtual.SMRT.RayCountLocal 4;r.Shadow.Virtual.SMRT.SamplesPerRayLocal 4;r.Lumen.ScreenProbeGather.TracingOctahedronResolution 4;r.Lumen.Reflections.DownsampleFactor 2"), TEXT(""), 0 },
};
/** Runs "cvar value;cvar value", remembering the previous values so they can be put back. */
void SurveyExecSet(UWorld* World, const TCHAR* Commands, TArray<TPair<FString, FString>>& Saved)
{
    TArray<FString> Parts; FString(Commands).ParseIntoArray(Parts, TEXT(";"), true);
    for (FString& Cmd : Parts)
    {
        Cmd.TrimStartAndEndInline();
        FString Name, Value; if (!Cmd.Split(TEXT(" "), &Name, &Value)) continue;
        if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(*Name)) Saved.Add({ Name, V->GetString() });
        GEngine->Exec(World, *Cmd);
    }
}
void SurveyApplyLights(FHouseDetailSurveyState& S, UWorld* World, int32 Mode)
{
    for (int32 K = 0; K < S.TouchedLights.Num(); ++K)
        if (ULightComponent* L = S.TouchedLights[K].Get()) { L->SetCastShadows(S.TouchedShadow[K]); L->SetVisibility(S.TouchedVisible[K]); L->MaxDrawDistance = S.TouchedDraw[K]; L->MarkRenderStateDirty(); }
    S.TouchedLights.Reset(); S.TouchedShadow.Reset(); S.TouchedVisible.Reset(); S.TouchedDraw.Reset();
    if (Mode == 0) return;
    for (TObjectIterator<ULightComponent> It; It; ++It)
    {
        ULightComponent* L = *It;
        if (!IsValid(L) || L->GetWorld() != World || !L->IsRegistered() || L->IsA<UDirectionalLightComponent>()) continue;
        S.TouchedLights.Add(L); S.TouchedShadow.Add(L->CastShadows); S.TouchedVisible.Add(L->IsVisible()); S.TouchedDraw.Add(L->MaxDrawDistance);
        if (Mode == 1) L->SetCastShadows(false);
        else if (Mode == 2 && !L->CastShadows) L->SetVisibility(false);
        else if (Mode == 4) { L->MaxDrawDistance = 2000.f; L->MaxDistanceFadeRange = 400.f; L->MarkRenderStateDirty(); }
        else if (Mode == 5) { L->MaxDrawDistance = 0.f; L->MarkRenderStateDirty(); }
    }
}
FVector SurveyPoint(const TSharedPtr<FJsonObject>& O)
{
    double X = 0, Y = 0, Z = 0;
    if (O.IsValid()) { O->TryGetNumberField(TEXT("x"), X); O->TryGetNumberField(TEXT("y"), Y); O->TryGetNumberField(TEXT("z"), Z); }
    return FVector(Z * 100, X * 100, Y * 100);
}
FString SurveyVec(const FVector& V) { return FString::Printf(TEXT("%.0f,%.0f,%.0f"), V.X, V.Y, V.Z); }

void SurveyCensus(UWorld* World, const FString& Dir)
{
    TArray<TSharedPtr<FJsonValue>> Actors, Lights, Props, Doors, Fixtures, Dupes;
    TMap<FString, int32> ClassCounts, TickClassCounts;
    int32 NumActors = 0, NumTicking = 0, NumTickComps = 0, NumPrims = 0, NumSim = 0, NumSimAwake = 0, NumSMC = 0;
    int64 Tris = 0;
    struct FPlaced { FString Mesh; FVector Loc; FString Owner; };
    TArray<FPlaced> PlacedMeshes;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* A = *It; if (!IsValid(A)) continue;
        ++NumActors;
        const FString Cls = A->GetClass()->GetName();
        ClassCounts.FindOrAdd(Cls)++;
        const bool bTick = A->IsActorTickEnabled() && A->PrimaryActorTick.bCanEverTick;
        if (bTick) { ++NumTicking; TickClassCounts.FindOrAdd(Cls)++; }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("label"), A->GetActorNameOrLabel());
        Row->SetStringField(TEXT("class"), Cls);
        Row->SetStringField(TEXT("loc"), SurveyVec(A->GetActorLocation()));
        FVector O, E; A->GetActorBounds(false, O, E);
        Row->SetStringField(TEXT("bmin"), SurveyVec(O - E)); Row->SetStringField(TEXT("bmax"), SurveyVec(O + E));
        if (A->IsHidden()) Row->SetBoolField(TEXT("hidden"), true);
        if (bTick) Row->SetBoolField(TEXT("tick"), true);
        if (A->GetIsReplicated()) Row->SetBoolField(TEXT("rep"), true);
        TArray<TSharedPtr<FJsonValue>> Comps;
        TInlineComponentArray<UActorComponent*> AllComps; A->GetComponents(AllComps);
        for (UActorComponent* C : AllComps)
        {
            if (!C) continue;
            if (C->IsComponentTickEnabled() && C->PrimaryComponentTick.bCanEverTick) ++NumTickComps;
            UPrimitiveComponent* P = Cast<UPrimitiveComponent>(C); if (!P) continue;
            ++NumPrims;
            TSharedPtr<FJsonObject> CR = MakeShared<FJsonObject>();
            CR->SetStringField(TEXT("c"), C->GetClass()->GetName());
            if (!P->IsVisible() || P->bHiddenInGame) CR->SetBoolField(TEXT("hidden"), true);
            if (P->GetCollisionEnabled() != ECollisionEnabled::NoCollision) CR->SetStringField(TEXT("col"), P->GetCollisionProfileName().ToString());
            if (P->IsSimulatingPhysics()) { ++NumSim; CR->SetBoolField(TEXT("sim"), true); if (P->IsAnyRigidBodyAwake()) { ++NumSimAwake; CR->SetBoolField(TEXT("awake"), true); } CR->SetNumberField(TEXT("mass"), P->GetMass()); }
            if (!P->CastShadow) CR->SetBoolField(TEXT("noshadow"), true);
            if (P->Mobility != EComponentMobility::Static) CR->SetStringField(TEXT("mob"), P->Mobility == EComponentMobility::Movable ? TEXT("movable") : TEXT("stationary"));
            if (UStaticMeshComponent* S = Cast<UStaticMeshComponent>(P))
            {
                ++NumSMC;
                if (UStaticMesh* M = S->GetStaticMesh())
                {
                    CR->SetStringField(TEXT("mesh"), M->GetName());
                    const FStaticMeshRenderData* RD = M->GetRenderData();
                    if (RD && RD->LODResources.Num() > 0)
                    {
                        int32 T = 0; for (const FStaticMeshSection& Sec : RD->LODResources[0].Sections) T += Sec.NumTriangles;
                        CR->SetNumberField(TEXT("tris"), T); CR->SetNumberField(TEXT("lods"), RD->LODResources.Num());
                        if (P->IsVisible() && !P->bHiddenInGame) Tris += T;
                    }
                    if (M->HasValidNaniteData()) CR->SetBoolField(TEXT("nanite"), true);
                    if (M->bAllowCPUAccess) CR->SetBoolField(TEXT("cpu"), true);
                    if (UInstancedStaticMeshComponent* I = Cast<UInstancedStaticMeshComponent>(S)) CR->SetNumberField(TEXT("inst"), I->GetInstanceCount());
                    if (P->IsVisible() && !P->bHiddenInGame) PlacedMeshes.Add({ M->GetName(), S->GetComponentLocation(), A->GetActorNameOrLabel() });
                }
                FString Mats; for (int32 K = 0; K < S->GetNumMaterials(); ++K) Mats += GetNameSafe(S->GetMaterial(K)) + TEXT(";");
                CR->SetStringField(TEXT("mats"), Mats);
                CR->SetStringField(TEXT("ext"), SurveyVec(S->Bounds.BoxExtent));
            }
            else if (UProceduralMeshComponent* PM = Cast<UProceduralMeshComponent>(P))
            {
                int32 T = 0; for (int32 K = 0; K < PM->GetNumSections(); ++K) if (const FProcMeshSection* Sec = PM->GetProcMeshSection(K)) T += Sec->ProcIndexBuffer.Num() / 3;
                CR->SetNumberField(TEXT("tris"), T); Tris += T;
                CR->SetStringField(TEXT("ext"), SurveyVec(PM->Bounds.BoxExtent));
            }
            else if (USkeletalMeshComponent* SK = Cast<USkeletalMeshComponent>(P))
            {
                CR->SetStringField(TEXT("mesh"), GetNameSafe(SK->GetSkeletalMeshAsset()));
            }
            Comps.Add(MakeShared<FJsonValueObject>(CR));
        }
        if (Comps.Num()) Row->SetArrayField(TEXT("comps"), Comps);
        Actors.Add(MakeShared<FJsonValueObject>(Row));
        if (AHouseInteractiveProp* Pr = Cast<AHouseInteractiveProp>(A))
        {
            TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
            R->SetStringField(TEXT("label"), A->GetActorNameOrLabel()); R->SetStringField(TEXT("id"), Pr->PropId.ToString()); R->SetStringField(TEXT("name"), Pr->PropName);
            R->SetStringField(TEXT("mesh"), GetNameSafe(Pr->IntactMesh)); R->SetStringField(TEXT("loc"), SurveyVec(A->GetActorLocation()));
            R->SetNumberField(TEXT("mass"), Pr->MassKg); R->SetBoolField(TEXT("movable"), Pr->bMovable); R->SetBoolField(TEXT("breakable"), Pr->bBreakable);
            R->SetBoolField(TEXT("wall"), Pr->bWallMounted); R->SetBoolField(TEXT("cloth"), Pr->IsClothFlag()); R->SetBoolField(TEXT("rolling"), Pr->IsRollingChair());
            R->SetBoolField(TEXT("broken_mesh"), Pr->BrokenMesh != nullptr); R->SetBoolField(TEXT("damaged_mesh"), Pr->DamagedMesh != nullptr);
            R->SetStringField(TEXT("carry"), FString::FromInt((int32)Pr->CarrySize));
            if (Pr->Body) { R->SetBoolField(TEXT("sim"), Pr->Body->IsSimulatingPhysics()); R->SetStringField(TEXT("ext"), SurveyVec(Pr->Body->Bounds.BoxExtent)); }
            Props.Add(MakeShared<FJsonValueObject>(R));
        }
        if (AExistingHouseDoor* D = Cast<AExistingHouseDoor>(A))
        {
            TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
            R->SetStringField(TEXT("label"), A->GetActorNameOrLabel()); R->SetStringField(TEXT("id"), D->DoorId.ToString()); R->SetStringField(TEXT("class"), Cls);
            R->SetStringField(TEXT("loc"), SurveyVec(A->GetActorLocation())); R->SetBoolField(TEXT("locked"), D->bLocked); R->SetStringField(TEXT("group"), D->DoorGroup.ToString());
            Doors.Add(MakeShared<FJsonValueObject>(R));
        }
        if (AHouseReactiveFixture* Fx = Cast<AHouseReactiveFixture>(A))
        {
            TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
            R->SetStringField(TEXT("label"), A->GetActorNameOrLabel()); R->SetStringField(TEXT("id"), Fx->FixtureId.ToString());
            R->SetNumberField(TEXT("kind"), (int32)Fx->FixtureKind); R->SetStringField(TEXT("loc"), SurveyVec(A->GetActorLocation()));
            Fixtures.Add(MakeShared<FJsonValueObject>(R));
        }
    }
    for (TObjectIterator<ULightComponent> It; It; ++It)
    {
        ULightComponent* L = *It;
        if (!IsValid(L) || L->GetWorld() != World || !L->IsRegistered()) continue;
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        R->SetStringField(TEXT("owner"), L->GetOwner() ? L->GetOwner()->GetActorNameOrLabel() : TEXT(""));
        R->SetStringField(TEXT("class"), L->GetClass()->GetName());
        R->SetStringField(TEXT("loc"), SurveyVec(L->GetComponentLocation()));
        R->SetNumberField(TEXT("intensity"), L->Intensity);
        R->SetBoolField(TEXT("shadows"), L->CastShadows);
        R->SetBoolField(TEXT("visible"), L->IsVisible() && !L->bHiddenInGame);
        R->SetStringField(TEXT("mob"), L->Mobility == EComponentMobility::Movable ? TEXT("movable") : (L->Mobility == EComponentMobility::Stationary ? TEXT("stationary") : TEXT("static")));
        if (UPointLightComponent* PL = Cast<UPointLightComponent>(L)) { R->SetNumberField(TEXT("radius"), PL->AttenuationRadius); R->SetNumberField(TEXT("units"), (int32)PL->IntensityUnits); }
        if (URectLightComponent* RL = Cast<URectLightComponent>(L)) R->SetNumberField(TEXT("radius"), RL->AttenuationRadius);
        Lights.Add(MakeShared<FJsonValueObject>(R));
    }
    // Same mesh placed twice at the same spot (z-fighting candidates).
    for (int32 I = 0; I < PlacedMeshes.Num(); ++I)
        for (int32 J = I + 1; J < PlacedMeshes.Num(); ++J)
            if (PlacedMeshes[I].Mesh == PlacedMeshes[J].Mesh && FVector::DistSquared(PlacedMeshes[I].Loc, PlacedMeshes[J].Loc) < 4.f)
            {
                TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
                R->SetStringField(TEXT("mesh"), PlacedMeshes[I].Mesh); R->SetStringField(TEXT("a"), PlacedMeshes[I].Owner); R->SetStringField(TEXT("b"), PlacedMeshes[J].Owner);
                R->SetStringField(TEXT("loc"), SurveyVec(PlacedMeshes[I].Loc));
                Dupes.Add(MakeShared<FJsonValueObject>(R));
            }
    TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
    Summary->SetNumberField(TEXT("actors"), NumActors); Summary->SetNumberField(TEXT("ticking_actors"), NumTicking); Summary->SetNumberField(TEXT("ticking_components"), NumTickComps);
    Summary->SetNumberField(TEXT("primitives"), NumPrims); Summary->SetNumberField(TEXT("static_mesh_components"), NumSMC); Summary->SetNumberField(TEXT("simulating"), NumSim); Summary->SetNumberField(TEXT("simulating_awake"), NumSimAwake);
    Summary->SetNumberField(TEXT("visible_triangles"), (double)Tris); Summary->SetNumberField(TEXT("lights"), Lights.Num());
    TSharedPtr<FJsonObject> CC = MakeShared<FJsonObject>(), TC = MakeShared<FJsonObject>();
    for (const auto& Kv : ClassCounts) CC->SetNumberField(Kv.Key, Kv.Value);
    for (const auto& Kv : TickClassCounts) TC->SetNumberField(Kv.Key, Kv.Value);
    TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetObjectField(TEXT("summary"), Summary); Report->SetObjectField(TEXT("classes"), CC); Report->SetObjectField(TEXT("ticking_classes"), TC);
    Report->SetArrayField(TEXT("props"), Props); Report->SetArrayField(TEXT("doors"), Doors); Report->SetArrayField(TEXT("fixtures"), Fixtures);
    Report->SetArrayField(TEXT("lights"), Lights); Report->SetArrayField(TEXT("duplicates"), Dupes); Report->SetArrayField(TEXT("actors"), Actors);
    FString Json; FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
    FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Dir, TEXT("census.json")));
    UE_LOG(LogTemp, Display, TEXT("HOUSE_SURVEY_CENSUS actors=%d ticking=%d prims=%d sim=%d tris=%lld lights=%d"), NumActors, NumTicking, NumPrims, NumSim, Tris, Lights.Num());

    // Optional focused diagnosis of the great room using the live player's shape.
    if (FParse::Param(FCommandLine::Get(), TEXT("GreatRoomCollisionScan")))
    {
        AStoryFirstPersonCharacter* Pawn = Cast<AStoryFirstPersonCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
        if (Pawn)
        {
            const float Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
            const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            FCollisionQueryParams GQ(SCENE_QUERY_STAT(GreatRoomCollisionScan), false, Pawn);
            FString Scan = FString::Printf(TEXT("capsule radius=%.2f half=%.2f\n"), Radius, Half);
            for (float X = -1400; X <= 1450; X += 25)
                for (float Y = -700; Y <= 1600; Y += 25)
                {
                    const FVector At(X, Y, Half + 3);
                    TArray<FOverlapResult> Hits;
                    World->OverlapMultiByChannel(Hits, At, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, Half), GQ);
                    for (const FOverlapResult& H : Hits)
                    {
                        UPrimitiveComponent* C = H.GetComponent();
                        if (!C || C->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block) continue;
                        Scan += FString::Printf(TEXT("%.0f,%.0f %s/%s hidden=%d loc=%s ext=%s\n"), X, Y,
                            H.GetActor() ? *H.GetActor()->GetActorNameOrLabel() : TEXT("none"), *C->GetName(),
                            !C->IsVisible() || C->bHiddenInGame || (C->GetOwner() && C->GetOwner()->IsHidden()),
                            *SurveyVec(C->Bounds.Origin), *SurveyVec(C->Bounds.BoxExtent));
                    }
                }
            FFileHelper::SaveStringToFile(Scan, *FPaths::Combine(Dir, TEXT("great-room-collision.txt")));
        }
    }

    for (const TCHAR* Name : { TEXT("r.LumenScene.SurfaceCache.CardTexelDensityScale"), TEXT("r.LumenScene.SurfaceCache.AtlasSize"), TEXT("r.LumenScene.SurfaceCache.CardMaxResolution"),
        TEXT("r.Shadow.Virtual.ResolutionLodBiasLocal"), TEXT("r.RayTracing.Enable"), TEXT("r.RayTracing"), TEXT("r.Lumen.ScreenProbeGather.DownsampleFactor"), TEXT("house.LightCullDistance"),
        TEXT("r.LumenScene.SurfaceCache.MeshCardsMaxDistance"), TEXT("r.LumenScene.CardCaptureMaxDistance") })
    {
        IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name);
        UE_LOG(LogTemp, Display, TEXT("HOUSE_SURVEY_CVAR %s = %s"), Name, V ? *V->GetString() : TEXT("<missing>"));
    }
    // Corridor scan: wall / floor / ceiling distances along each hallway spine.
    struct FSpine { const TCHAR* Name; FVector A, B; FVector Side; };
    const FSpine Spines[] = {
        { TEXT("S03_LongHall"), FVector(-2212, -1450, 118), FVector(-2212, 1050, 118), FVector(1, 0, 0) },
        { TEXT("S03_ShortHall"), FVector(-1000, -715, 118), FVector(-2300, -715, 118), FVector(0, 1, 0) },
        { TEXT("S03_Branch"), FVector(-1611, -500, 118), FVector(-1611, -1100, 118), FVector(1, 0, 0) },
        { TEXT("S05_RearHall"), FVector(-2195, -1500, 420), FVector(-2195, 1100, 420), FVector(1, 0, 0) },
        { TEXT("S05_FarHall"), FVector(2008, -1500, 420), FVector(2008, 1500, 420), FVector(1, 0, 0) },
        { TEXT("Gallery"), FVector(-1350, -725, 425), FVector(1350, -725, 425), FVector(0, 1, 0) },
        { TEXT("UpperShortHall"), FVector(-1150, -725, 420), FVector(-2250, -725, 420), FVector(0, 1, 0) },
        { TEXT("UpperNorthLink"), FVector(1150, -725, 420), FVector(2100, -725, 420), FVector(0, 1, 0) },
        { TEXT("S03_BathHall"), FVector(-1700, -400, 118), FVector(-1700, 900, 118), FVector(1, 0, 0) },
        { TEXT("Basement_StairCorridor"), FVector(-1863, 200, -220), FVector(-1863, 900, -220), FVector(1, 0, 0) },
    };
    FString Scan;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(HouseCorridorScan), false);
    for (const FSpine& Sp : Spines)
    {
        const float Len = (Sp.B - Sp.A).Size(); const FVector Along = (Sp.B - Sp.A).GetSafeNormal();
        for (float T = 0; T <= Len; T += 25.f)
        {
            const FVector P = Sp.A + Along * T;
            FString Row = FString::Printf(TEXT("%s t=%.0f p=%s"), Sp.Name, T, *SurveyVec(P));
            const FVector Dirs[4] = { Sp.Side, -Sp.Side, FVector(0, 0, 1), FVector(0, 0, -1) };
            const TCHAR* Tags[4] = { TEXT("+"), TEXT("-"), TEXT("up"), TEXT("dn") };
            for (int32 K = 0; K < 4; ++K)
            {
                FHitResult H;
                if (World->LineTraceSingleByChannel(H, P, P + Dirs[K] * 1500.f, ECC_Visibility, Q))
                    Row += FString::Printf(TEXT(" %s=%.0f(%s)"), Tags[K], H.Distance, *GetNameSafe(H.GetComponent()).Left(28));
                else Row += FString::Printf(TEXT(" %s=none"), Tags[K]);
            }
            Scan += Row + TEXT("\n");
        }
    }
    FFileHelper::SaveStringToFile(Scan, *FPaths::Combine(Dir, TEXT("corridors.txt")));
    // Stairwell scan: every walkable surface in each column of the two photographed stairwells (treads, landings, floors).
    struct FStairBox { const TCHAR* Name; FVector Min, Max; };
    const FStairBox StairBoxes[] = {
        { TEXT("S03_UpperStair"), FVector(-2340, 860, -120), FVector(-1420, 1280, 560) },
        { TEXT("S03_BasementStair"), FVector(-2340, 860, -480), FVector(-1420, 1280, 60) },
    };
    FString Stairs;
    for (const FStairBox& SB : StairBoxes)
    {
        for (float X = SB.Min.X; X <= SB.Max.X; X += 20.f)
            for (float Y = SB.Min.Y; Y <= SB.Max.Y; Y += 20.f)
            {
                FString Row = FString::Printf(TEXT("%s x=%.0f y=%.0f"), SB.Name, X, Y);
                float Z = SB.Max.Z; int32 Hits = 0;
                while (Z > SB.Min.Z && Hits < 12)
                {
                    FHitResult H;
                    if (!World->LineTraceSingleByChannel(H, FVector(X, Y, Z), FVector(X, Y, SB.Min.Z), ECC_Visibility, Q)) break;
                    if (H.ImpactNormal.Z > .5f) Row += FString::Printf(TEXT(" z=%.0f(%s)"), H.ImpactPoint.Z, *GetNameSafe(H.GetComponent()).Left(22));
                    else Row += FString::Printf(TEXT(" u=%.0f"), H.ImpactPoint.Z);
                    Z = H.ImpactPoint.Z - 3.f; ++Hits;
                }
                Stairs += Row + TEXT("\n");
            }
    }
    FFileHelper::SaveStringToFile(Stairs, *FPaths::Combine(Dir, TEXT("stairs.txt")));
}

void SurveyLoadLayout(FHouseDetailSurveyState& S)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("HouseReview/house_walkthrough.json")))) return;
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return;
    const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
    if (Root->TryGetArrayField(TEXT("landings"), Arr))
        for (const TSharedPtr<FJsonValue>& V : *Arr)
        {
            const TSharedPtr<FJsonObject> O = V->AsObject(); if (!O.IsValid()) continue;
            FSurveyProbe P; P.Name = O->GetStringField(TEXT("name")); P.Feet = SurveyPoint(O->GetObjectField(TEXT("feet")));
            P.Dir = SurveyPoint(O->GetObjectField(TEXT("walkDirection"))).GetSafeNormal();
            if (P.Name.Contains(TEXT("porch"))) continue;      // walking the porch trips the chapter exit
            S.Probes.Add(P);
        }
    if (Root->TryGetArrayField(TEXT("views"), Arr))
        for (const TSharedPtr<FJsonValue>& V : *Arr)
        {
            const TSharedPtr<FJsonObject> O = V->AsObject(); if (!O.IsValid()) continue;
            FSurveyView W; W.Name = O->GetStringField(TEXT("name")); W.Pos = SurveyPoint(O->GetObjectField(TEXT("position"))); W.Target = SurveyPoint(O->GetObjectField(TEXT("target")));
            double Fov = 70; O->TryGetNumberField(TEXT("fieldOfView"), Fov); W.Fov = (float)Fov;
            S.Views.Add(W);
        }
}

void SurveyFlushProbe(FHouseDetailSurveyState& S, const FString& Name, bool bWalk)
{
    if (S.Frames.Num() == 0) return;
    TArray<float> Dts; float SumDt = 0, SumG = 0, SumR = 0, SumGpu = 0, MaxDt = 0; int32 Hitches = 0;
    for (const FSurveyFrame& F : S.Frames) { Dts.Add(F.Dt); SumDt += F.Dt; SumG += F.Game; SumR += F.Render; SumGpu += F.Gpu; MaxDt = FMath::Max(MaxDt, F.Dt); if (F.Dt > 40.f) ++Hitches; }
    Dts.Sort();
    const int32 N = S.Frames.Num();
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetStringField(TEXT("probe"), Name); R->SetBoolField(TEXT("walking"), bWalk); R->SetNumberField(TEXT("frames"), N);
    R->SetNumberField(TEXT("avg_ms"), SumDt / N); R->SetNumberField(TEXT("p95_ms"), Dts[FMath::Clamp(int32(N * .95f), 0, N - 1)]); R->SetNumberField(TEXT("max_ms"), MaxDt);
    R->SetNumberField(TEXT("game_ms"), SumG / N); R->SetNumberField(TEXT("render_ms"), SumR / N); R->SetNumberField(TEXT("gpu_ms"), SumGpu / N); R->SetNumberField(TEXT("hitches"), Hitches);
    S.ProbeRows.Add(MakeShared<FJsonValueObject>(R));
    UE_LOG(LogTemp, Display, TEXT("HOUSE_SURVEY_PROBE %s walk=%d avg=%.1f p95=%.1f max=%.1f game=%.1f render=%.1f gpu=%.1f"), *Name, bWalk ? 1 : 0, SumDt / N, Dts[FMath::Clamp(int32(N * .95f), 0, N - 1)], MaxDt, SumG / N, SumR / N, SumGpu / N);
    S.Frames.Reset();
}
}

void TickHouseDetailSurvey(AStoryFirstPersonCharacter* Pawn)
{
    if (!Pawn->HasAuthority() || !Pawn->IsLocallyControlled()) return;
    UWorld* World = Pawn->GetWorld();
    if (!World || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
    if (!PC) return;
    static FHouseDetailSurveyState S;
    const double Now = World->GetTimeSeconds();
    if (S.World.Get() != World)
    {
        S = FHouseDetailSurveyState(); S.World = World; S.Started = S.StepStarted = Now;
        S.Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HouseDetailSurvey")));
        IFileManager::Get().MakeDirectory(*S.Dir, true);
        SurveyLoadLayout(S);
        // Extra probes along the photographed hallways.
        S.Probes.Add({ TEXT("Upper rear hall mid"), FVector(-2195, 200, 320), FVector(0, -1, 0) });
        S.Probes.Add({ TEXT("Upper far hall mid"), FVector(2008, 0, 330), FVector(0, 1, 0) });
        S.Probes.Add({ TEXT("Gallery mid"), FVector(0, -725, 320), FVector(1, 0, 0) });
        S.Probes.Add({ TEXT("Black hall centre"), FVector(0, 0, -320), FVector(0, 1, 0) });
        // Hallway dressing views.
        S.Views.Add({ TEXT("hw01_long_from_stair"), FVector(-2212, 900, 150), FVector(-2212, -1400, 110), 80 });
        S.Views.Add({ TEXT("hw02_long_from_bedroom"), FVector(-2212, -1300, 150), FVector(-2212, 1000, 110), 80 });
        S.Views.Add({ TEXT("hw03_long_east_wall"), FVector(-2300, -1150, 150), FVector(-2067, -1000, 130), 85 });
        S.Views.Add({ TEXT("hw04_long_west_wall"), FVector(-2120, -600, 150), FVector(-2342, -850, 120), 85 });
        S.Views.Add({ TEXT("hw05_short_hall"), FVector(-2040, -715, 150), FVector(-1350, -715, 120), 85 });
        S.Views.Add({ TEXT("hw06_short_panel"), FVector(-1700, -820, 150), FVector(-1480, -560, 140), 85 });
        S.Views.Add({ TEXT("hw07_rear_from_landing"), FVector(-2195, 800, 430), FVector(-2195, -1400, 400), 80 });
        S.Views.Add({ TEXT("hw08_rear_nook"), FVector(-2250, -500, 430), FVector(-2090, -1150, 380), 85 });
        S.Views.Add({ TEXT("hw09_rear_204"), FVector(-2090, -150, 430), FVector(-2320, -750, 420), 85 });
        S.Views.Add({ TEXT("hw10_landing_bench"), FVector(-2110, 880, 440), FVector(-2284, 1030, 340), 85 });
        S.Views.Add({ TEXT("hw11_far_hall"), FVector(2008, 350, 430), FVector(2008, -1400, 400), 80 });
        S.Views.Add({ TEXT("hw12_far_cabinet"), FVector(2110, -250, 430), FVector(1898, -650, 400), 85 });
        S.Views.Add({ TEXT("hw13_gallery_west"), FVector(-300, -725, 440), FVector(-1250, -725, 420), 80 });
        S.Views.Add({ TEXT("hw14_upper_short"), FVector(-1250, -725, 440), FVector(-2100, -725, 420), 85 });
        S.Views.Add({ TEXT("hw15_north_link"), FVector(1200, -725, 440), FVector(2000, -725, 420), 85 });
        S.Views.Add({ TEXT("hw16_rear_toward_end"), FVector(-2195, -600, 430), FVector(-2195, -1450, 380), 85 });
        S.Views.Add({ TEXT("hw17_long_exit_junction"), FVector(-2212, -300, 150), FVector(-2212, -1450, 130), 80 });
        S.Views.Add({ TEXT("hw18_gallery_checkpoint"), FVector(-700, -725, 440), FVector(800, -725, 420), 80 });
        S.Views.Add({ TEXT("hw19_rear_dalmatian"), FVector(-2080, -880, 440), FVector(-2320, -1090, 430), 75 });
        S.Views.Add({ TEXT("hw20_nook_front"), FVector(-2300, -820, 430), FVector(-2080, -950, 370), 80 });
        S.Views.Add({ TEXT("hw21_gallery_fixtures"), FVector(-700, -760, 440), FVector(-1200, -560, 420), 80 });
        S.Views.Add({ TEXT("hw22_view05_compare"), FVector(-2205, 571, 163), FVector(-2205, -1228, 163), 57 });
        S.Views.Add({ TEXT("hw23_landing_from_stairs"), FVector(-2060, 1050, 430), FVector(-2290, 1000, 330), 80 });
        S.Views.Add({ TEXT("hw24_stair_b_rail"), FVector(-2040, 960, 455), FVector(-1560, 890, 269), 80 });
        S.Views.Add({ TEXT("hw25_mid_landing_clutter"), FVector(-1620, 990, 319), FVector(-1470, 1150, 175), 80 });
        S.Views.Add({ TEXT("hw26_stair_c_rail"), FVector(-2130, 960, 151), FVector(-1600, 890, -65), 80 });
    }
    if (S.Step < 0) return;
    const float Age = Now - S.StepStarted;
    auto Next = [&](int32 N) { S.Step = N; S.StepStarted = Now; };
    if (S.Step == 0)
    {
        if (Age < 6.f) return;
        SurveyCensus(World, S.Dir);
        if (FParse::Param(FCommandLine::Get(), TEXT("CensusOnly"))) { S.Step = -1; FPlatformMisc::RequestExitWithStatus(false, 0); return; }
        if (PC->MyHUD) PC->MyHUD->bShowHUD = false;
        if (FParse::Param(FCommandLine::Get(), TEXT("MatrixOnly")))
        {
            S.Views.Reset(); S.Probes.Reset();
            S.Index = 0; S.Placed = -1; Next(2); S.Spot = 0; return;
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("HallwayViewsOnly")))
        {
            S.Views.RemoveAll([](const FSurveyView& V) { return !V.Name.StartsWith(TEXT("hw")); });
            S.Index = 0; S.Placed = -1; Next(2); S.Spot = 3; return;
        }
        S.Index = 0; S.Placed = -1; Next(1); return;
    }
    const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (S.Step == 1)
    {
        // Frame-time probe: stand, then walk, at every landing.
        if (S.Index >= S.Probes.Num()) { Next(2); S.Index = 0; S.Placed = -1; return; }
        const FSurveyProbe& P = S.Probes[S.Index];
        if (S.Placed != S.Index)
        {
            S.Placed = S.Index; S.StepStarted = Now; S.Frames.Reset(); S.bWalking = false;
            Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking); Pawn->SetActorEnableCollision(true);
            Pawn->SetActorLocation(P.Feet + FVector(0, 0, Half + 4.f), false, nullptr, ETeleportType::TeleportPhysics);
            PC->SetControlRotation(P.Dir.Rotation());
            return;
        }
        if (Age > 1.f)
        {
            const FSurveyFrame F{ FApp::GetDeltaTime() * 1000.f, FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()) };
            S.Frames.Add(F);
        }
        if (!S.bWalking && Age > 3.f) { SurveyFlushProbe(S, P.Name, false); S.bWalking = true; }
        if (S.bWalking)
        {
            Pawn->AddMovementInput(P.Dir, 1.f);
            if (Age > 5.f) { SurveyFlushProbe(S, P.Name, true); ++S.Index; }
        }
        return;
    }
    if (S.Step == 2)
    {
        // Rendering A/B matrix at three spots.
        const FVector Spots[3] = { FVector(1076, 567, 5), FVector(-2212, -258, 8), FVector(-1026, 639, -324) };
        const FVector Dirs[3] = { FVector(-1, -0.3f, 0), FVector(0, 1, 0), FVector(1, -0.6f, 0) };
        const TCHAR* SpotNames[3] = { TEXT("great_room"), TEXT("long_hall"), TEXT("black_hall") };
        const int32 NumConfigs = UE_ARRAY_COUNT(SurveyConfigs);
        if (S.Spot >= 3) { Next(5); return; }
        if (S.Placed != S.Spot * 100 + S.Config)
        {
            S.Placed = S.Spot * 100 + S.Config; S.StepStarted = Now; S.Frames.Reset();
            if (S.Config == 0)
            {
                Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking); Pawn->SetActorEnableCollision(true);
                Pawn->SetActorLocation(Spots[S.Spot] + FVector(0, 0, Half + 4.f), false, nullptr, ETeleportType::TeleportPhysics);
                PC->SetControlRotation(Dirs[S.Spot].Rotation());
            }
            const FSurveyConfig& C = SurveyConfigs[S.Config];
            S.SavedCVars.Reset();
            if (C.Apply[0]) SurveyExecSet(World, C.Apply, S.SavedCVars);
            SurveyApplyLights(S, World, C.LightMode);
            return;
        }
        if (Age > 1.5f) S.Frames.Add({ FApp::GetDeltaTime() * 1000.f, FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()) });
        if (Age > 3.5f)
        {
            const FSurveyConfig& C = SurveyConfigs[S.Config];
            if (S.Frames.Num())
            {
                float SumDt = 0, SumGpu = 0, SumGame = 0; for (const FSurveyFrame& F : S.Frames) { SumDt += F.Dt; SumGpu += F.Gpu; SumGame += F.Game; }
                TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
                R->SetStringField(TEXT("spot"), SpotNames[S.Spot]); R->SetStringField(TEXT("config"), C.Name);
                R->SetNumberField(TEXT("avg_ms"), SumDt / S.Frames.Num()); R->SetNumberField(TEXT("gpu_ms"), SumGpu / S.Frames.Num()); R->SetNumberField(TEXT("game_ms"), SumGame / S.Frames.Num());
                S.MatrixRows.Add(MakeShared<FJsonValueObject>(R));
                UE_LOG(LogTemp, Display, TEXT("HOUSE_SURVEY_MATRIX %s %s avg=%.1f gpu=%.1f game=%.1f"), SpotNames[S.Spot], C.Name, SumDt / S.Frames.Num(), SumGpu / S.Frames.Num(), SumGame / S.Frames.Num());
                if (S.Config == 0 || S.Config == 2 || S.Config == 6 || S.Config == 12)
                    FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir, FString::Printf(TEXT("matrix_%s_%s.png"), SpotNames[S.Spot], C.Name)), false, false);
            }
            if (C.Restore[0]) GEngine->Exec(World, C.Restore);
            else for (const TPair<FString, FString>& Prev : S.SavedCVars) GEngine->Exec(World, *(Prev.Key + TEXT(" ") + Prev.Value));
            SurveyApplyLights(S, World, 0);
            S.Frames.Reset();
            FString MatrixSet; FParse::Value(FCommandLine::Get(), TEXT("MatrixSet="), MatrixSet);
            do { ++S.Config; } while (S.Config < NumConfigs && !MatrixSet.IsEmpty() && !FString(SurveyConfigs[S.Config].Name).StartsWith(MatrixSet + TEXT("_")));
            if (S.Config >= NumConfigs) { S.Config = 0; ++S.Spot; }
            S.Placed = -1; S.StepStarted = Now;
        }
        return;
    }
    if (S.Step == 5)
    {
        TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>(); Report->SetArrayField(TEXT("probes"), S.ProbeRows); Report->SetArrayField(TEXT("matrix"), S.MatrixRows);
        FString Json; FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json, *FPaths::Combine(S.Dir, TEXT("perf.json")));
        FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        S.Cam = World->SpawnActor<ACameraActor>(FVector::ZeroVector, FRotator::ZeroRotator, SP);
        if (S.Cam.IsValid())
        {
            UCameraComponent* CC = S.Cam->GetCameraComponent(); CC->bConstrainAspectRatio = false;
            if (Pawn->FirstPersonCamera) { CC->PostProcessSettings = Pawn->FirstPersonCamera->PostProcessSettings; CC->PostProcessBlendWeight = Pawn->FirstPersonCamera->PostProcessBlendWeight; }
        }
        Pawn->SetActorHiddenInGame(true);
        { TArray<AActor*> Attached; Pawn->GetAttachedActors(Attached, true, true); for (AActor* A : Attached) if (A) A->SetActorHiddenInGame(true); }
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying); Pawn->SetActorEnableCollision(false);
        Pawn->SetActorLocation(FVector(0, 0, 10000), false, nullptr, ETeleportType::TeleportPhysics);
        S.Index = 0; S.Placed = -1; Next(3); return;
    }
    if (S.Step == 3)
    {
        if (S.Index >= S.Views.Num()) { Next(4); return; }
        const FSurveyView& V = S.Views[S.Index];
        if (S.Placed != S.Index)
        {
            if (FScreenshotRequest::IsScreenshotRequested()) return;
            S.Placed = S.Index; S.StepStarted = Now;
            if (ACameraActor* C = S.Cam.Get())
            {
                C->SetActorLocationAndRotation(V.Pos, (V.Target - V.Pos).Rotation());
                C->GetCameraComponent()->SetFieldOfView(V.Fov);
                if (PC->GetViewTarget() != C) PC->SetViewTarget(C);
            }
            return;
        }
        if (Age > 1.3f && !FScreenshotRequest::IsScreenshotRequested())
        {
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir, FString::Printf(TEXT("view_%s.png"), *V.Name)), false, false);
            ++S.Index;
        }
        return;
    }
    if (S.Step == 4)
    {
        if (FScreenshotRequest::IsScreenshotRequested() || Age < 1.f) return;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_SURVEY_DONE views=%d probes=%d seconds=%.0f"), S.Views.Num(), S.ProbeRows.Num(), Now - S.Started);
        S.Step = -1;
        FPlatformMisc::RequestExitWithStatus(false, 0);
    }
}
#else
void TickHouseDetailSurvey(AStoryFirstPersonCharacter*) {}
#endif
