#include "HouseBlackHall.h"

#if !UE_BUILD_SHIPPING
#include "StoryCampaign.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "EngineUtils.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "StaticMeshResources.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Camera/CameraActor.h"
#include "GameFramework/HUD.h"
#include "Components/CapsuleComponent.h"
#include "HouseBlackHallLook.h"

namespace
{
struct FBlackHallSurvey
{
    TWeakObjectPtr<UWorld> World; double Started=0, StepStarted=0; bool bScanned=false; int32 Index=0, Placed=-1;
    FString Dir; TArray<FVector> Views; TArray<FRotator> Rots; TArray<FString> Names;
};
const FBox HallSurveyRegion(FVector(-1800,-1200,-380),FVector(2000,1200,40));
}


static void BlackHallCensus(UWorld* World, APawn* Pawn, const FString& Dir)
{
    const FBox Hall(FVector(-1300,-900,-345),FVector(1700,900,-30));
    TArray<TSharedPtr<FJsonValue>> Rows;
    for(TObjectIterator<UStaticMeshComponent> It;It;++It)
    {
        UStaticMeshComponent* C=*It;
        if(!IsValid(C) || C->GetWorld()!=World || !C->IsRegistered() || !C->GetStaticMesh() || C->IsA<UInstancedStaticMeshComponent>())continue;
        if(!C->Bounds.GetBox().Intersect(Hall))continue;
        UStaticMesh* Mesh=C->GetStaticMesh();
        const FStaticMeshRenderData* RD=Mesh->GetRenderData();
        if(!RD || RD->LODResources.Num()==0)continue;
        const FStaticMeshLODResources& LOD=RD->LODResources[0];
        const FTransform T=C->GetComponentTransform();
        TSharedPtr<FJsonObject> Row=MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("owner"),C->GetOwner()?C->GetOwner()->GetActorNameOrLabel():TEXT(""));
        Row->SetStringField(TEXT("mesh"),Mesh->GetName());
        Row->SetBoolField(TEXT("visible"),C->IsVisible()&&!C->bHiddenInGame);
        Row->SetBoolField(TEXT("nanite"),Mesh->HasValidNaniteData());
        Row->SetBoolField(TEXT("cpu"),Mesh->bAllowCPUAccess);
        Row->SetStringField(TEXT("collision"),UEnum::GetValueAsString(C->GetCollisionEnabled()));
        TArray<TSharedPtr<FJsonValue>> Secs;
        const int32 NumIdx=LOD.IndexBuffer.GetNumIndices();
        const bool bHasPos=LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices()>0;
        for(int32 S=0;S<LOD.Sections.Num();++S)
        {
            const FStaticMeshSection& Sec=LOD.Sections[S];
            TSharedPtr<FJsonObject> SR=MakeShared<FJsonObject>();
            SR->SetNumberField(TEXT("section"),S);
            SR->SetStringField(TEXT("material"),GetNameSafe(C->GetMaterial(Sec.MaterialIndex)));
            SR->SetNumberField(TEXT("tris"),Sec.NumTriangles);
            // Classify triangles inside the hall by facing direction.
            FBox Dir6[7]; int32 Count6[7]={0,0,0,0,0,0,0}; for(FBox& B:Dir6)B.Init();
            int32 Inside=0;
            if(bHasPos && NumIdx>0)
            for(uint32 Tri=0;Tri<Sec.NumTriangles;++Tri)
            {
                const uint32 I0=LOD.IndexBuffer.GetIndex(Sec.FirstIndex+Tri*3),I1=LOD.IndexBuffer.GetIndex(Sec.FirstIndex+Tri*3+1),I2=LOD.IndexBuffer.GetIndex(Sec.FirstIndex+Tri*3+2);
                const FVector A=T.TransformPosition(FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(I0)));
                const FVector B=T.TransformPosition(FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(I1)));
                const FVector Cc=T.TransformPosition(FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(I2)));
                const FVector Ctr=(A+B+Cc)/3.f;
                if(!Hall.IsInside(Ctr))continue;
                ++Inside;
                const FVector N=FVector::CrossProduct(B-A,Cc-A).GetSafeNormal();
                int32 K=6;
                if(FMath::Abs(N.X)>.9f)K=N.X>0?0:1; else if(FMath::Abs(N.Y)>.9f)K=N.Y>0?2:3; else if(FMath::Abs(N.Z)>.9f)K=N.Z>0?4:5;
                Dir6[K]+=A;Dir6[K]+=B;Dir6[K]+=Cc;++Count6[K];
            }
            SR->SetNumberField(TEXT("inside"),Inside);
            const TCHAR* Names[7]={TEXT("px"),TEXT("nx"),TEXT("py"),TEXT("ny"),TEXT("pz"),TEXT("nz"),TEXT("other")};
            for(int32 K=0;K<7;++K) if(Count6[K]) SR->SetStringField(Names[K],FString::Printf(TEXT("%d [%s]-[%s]"),Count6[K],*Dir6[K].Min.ToCompactString(),*Dir6[K].Max.ToCompactString()));
            Secs.Add(MakeShared<FJsonValueObject>(SR));
        }
        Row->SetArrayField(TEXT("sections"),Secs);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    // Lights
    TArray<TSharedPtr<FJsonValue>> Lights;
    for(TObjectIterator<ULightComponent> It;It;++It)
    {
        ULightComponent* L=*It;
        if(!IsValid(L) || L->GetWorld()!=World || !L->IsRegistered())continue;
        const FVector P=L->GetComponentLocation();
        if(!Hall.ExpandBy(200).IsInside(P))continue;
        TSharedPtr<FJsonObject> R=MakeShared<FJsonObject>();
        R->SetStringField(TEXT("owner"),L->GetOwner()?L->GetOwner()->GetActorNameOrLabel():TEXT(""));
        R->SetStringField(TEXT("class"),L->GetClass()->GetName());
        R->SetStringField(TEXT("loc"),P.ToCompactString());
        R->SetStringField(TEXT("rot"),L->GetComponentRotation().ToCompactString());
        R->SetNumberField(TEXT("intensity"),L->Intensity);
        R->SetStringField(TEXT("color"),L->GetLightColor().ToString());
        R->SetBoolField(TEXT("shadows"),L->CastShadows);
        R->SetBoolField(TEXT("visible"),L->IsVisible());
        if(UPointLightComponent* PL=Cast<UPointLightComponent>(L)){R->SetNumberField(TEXT("radius"),PL->AttenuationRadius);R->SetNumberField(TEXT("units"),(int32)PL->IntensityUnits);}
        if(URectLightComponent* RL=Cast<URectLightComponent>(L)){R->SetNumberField(TEXT("w"),RL->SourceWidth);R->SetNumberField(TEXT("h"),RL->SourceHeight);R->SetNumberField(TEXT("radius"),RL->AttenuationRadius);}
        Lights.Add(MakeShared<FJsonValueObject>(R));
    }
    // Material parameter names for the house master materials.
    TSharedPtr<FJsonObject> Params=MakeShared<FJsonObject>();
    for(const TCHAR* Path:{TEXT("/Game/SeniorSendOff/House/Materials/M_House_Textured.M_House_Textured"),TEXT("/Game/SeniorSendOff/House/Materials/M_House_Plain.M_House_Plain"),
        TEXT("/Game/SeniorSendOff/House/Materials/M_House_Glass.M_House_Glass"),TEXT("/Game/SeniorSendOff/House/Materials/M_House_Normal.M_House_Normal"),
        TEXT("/Game/SeniorSendOff/House/Materials/MI_SSO_BlackBlock.MI_SSO_BlackBlock"),TEXT("/Game/SeniorSendOff/House/Materials/MI_SSO_BHSealedConcrete.MI_SSO_BHSealedConcrete"),
        TEXT("/Game/SeniorSendOff/House/Materials/MI_SSO_BHLightNeutral.MI_SSO_BHLightNeutral"),TEXT("/Game/SeniorSendOff/House/Materials/MI_SSO_GalleryExitRed.MI_SSO_GalleryExitRed")})
    {
        UMaterialInterface* M=LoadObject<UMaterialInterface>(nullptr,Path);
        if(!M){Params->SetStringField(Path,TEXT("missing"));continue;}
        FString Out;
        TArray<FMaterialParameterInfo> Info;TArray<FGuid> Ids;
        M->GetAllScalarParameterInfo(Info,Ids);for(auto& I:Info){float V=0;M->GetScalarParameterValue(I,V);Out+=FString::Printf(TEXT("S:%s=%.3f "),*I.Name.ToString(),V);}
        M->GetAllVectorParameterInfo(Info,Ids);for(auto& I:Info){FLinearColor V;M->GetVectorParameterValue(I,V);Out+=FString::Printf(TEXT("V:%s=%s "),*I.Name.ToString(),*V.ToString());}
        M->GetAllTextureParameterInfo(Info,Ids);for(auto& I:Info){UTexture* V=nullptr;M->GetTextureParameterValue(I,V);Out+=FString::Printf(TEXT("T:%s=%s "),*I.Name.ToString(),*GetNameSafe(V));}
        Out+=FString::Printf(TEXT(" blend=%d twosided=%d"),(int32)M->GetBlendMode(),M->IsTwoSided()?1:0);
        Params->SetStringField(Path,Out);
    }
    // Wall planes via horizontal traces.
    TArray<TSharedPtr<FJsonValue>> Traces;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(BlackHallCensus),true,Pawn);
    for(float Z:{-320.f,-250.f,-150.f,-90.f,-60.f})
    for(float X=-1200;X<=1300;X+=50)
    for(int32 D=0;D<2;++D)
    {
        FHitResult H;const FVector S(X,0,Z),E(X,D?-1500:1500,Z);
        TSharedPtr<FJsonObject> R=MakeShared<FJsonObject>();
        R->SetStringField(TEXT("ray"),FString::Printf(TEXT("x=%.0f z=%.0f dir=%s"),X,Z,D?TEXT("-y"):TEXT("+y")));
        if(World->LineTraceSingleByChannel(H,S,E,ECC_Visibility,Q)){R->SetNumberField(TEXT("hit"),H.ImpactPoint.Y);R->SetStringField(TEXT("comp"),GetNameSafe(H.GetComponent())+TEXT("@")+(H.GetActor()?H.GetActor()->GetActorNameOrLabel():TEXT("")));}
        Traces.Add(MakeShared<FJsonValueObject>(R));
    }
    for(float Z:{-320.f,-250.f,-150.f,-90.f,-60.f})
    for(float Y=-850;Y<=850;Y+=25)
    for(int32 D=0;D<2;++D)
    {
        FHitResult H;const FVector S(0,Y,Z),E(D?-1800:1900,Y,Z);
        TSharedPtr<FJsonObject> R=MakeShared<FJsonObject>();
        R->SetStringField(TEXT("ray"),FString::Printf(TEXT("y=%.0f z=%.0f dir=%s"),Y,Z,D?TEXT("-x"):TEXT("+x")));
        if(World->LineTraceSingleByChannel(H,S,E,ECC_Visibility,Q)){R->SetNumberField(TEXT("hit"),H.ImpactPoint.X);R->SetStringField(TEXT("comp"),GetNameSafe(H.GetComponent())+TEXT("@")+(H.GetActor()?H.GetActor()->GetActorNameOrLabel():TEXT("")));}
        Traces.Add(MakeShared<FJsonValueObject>(R));
    }
    TSharedPtr<FJsonObject> Report=MakeShared<FJsonObject>();
    Report->SetArrayField(TEXT("meshes"),Rows);Report->SetArrayField(TEXT("lights"),Lights);Report->SetObjectField(TEXT("params"),Params);Report->SetArrayField(TEXT("traces"),Traces);
    FString Json;FJsonSerializer::Serialize(Report.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
    FFileHelper::SaveStringToFile(Json,*FPaths::Combine(Dir,TEXT("census.json")));
}

void TickHouseBlackHallSurvey(AStoryFirstPersonCharacter* Pawn)
{
    if(!Pawn->HasAuthority() || !Pawn->IsLocallyControlled())return;
    UWorld* World=Pawn->GetWorld();
    if(!World || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    APlayerController* PC=Cast<APlayerController>(Pawn->GetController());
    if(!PC || !Pawn->FirstPersonCamera)return;
    static FBlackHallSurvey S;
    const double Now=World->GetTimeSeconds();
    if(S.World.Get()!=World){S=FBlackHallSurvey();S.World=World;S.Started=S.StepStarted=Now;
        S.Dir=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("HouseBlackHallSurvey")));IFileManager::Get().MakeDirectory(*S.Dir,true);}
    if(S.Index<0)return;
    if(!S.bScanned)
    {
        if(Now-S.StepStarted<3.f)return;
        S.bScanned=true;
        BlackHallCensus(World,Pawn,S.Dir);
        if(FParse::Param(FCommandLine::Get(),TEXT("CensusOnly"))){S.Index=-1;FPlatformMisc::RequestExitWithStatus(false,0);return;}
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(TObjectIterator<UPrimitiveComponent> It;It;++It)
        {
            UPrimitiveComponent* C=*It;
            if(!IsValid(C) || C->GetWorld()!=World || !C->IsRegistered())continue;
            const FBox B=C->Bounds.GetBox();
            if(!B.Intersect(HallSurveyRegion))continue;
            TSharedPtr<FJsonObject> Row=MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("owner"),GetNameSafe(C->GetOwner()));
            Row->SetStringField(TEXT("label"),C->GetOwner()?C->GetOwner()->GetActorNameOrLabel():TEXT(""));
            Row->SetStringField(TEXT("owner_class"),C->GetOwner()?C->GetOwner()->GetClass()->GetName():TEXT(""));
            Row->SetStringField(TEXT("component"),C->GetName());
            Row->SetStringField(TEXT("class"),C->GetClass()->GetName());
            if(UStaticMeshComponent* M=Cast<UStaticMeshComponent>(C))
            {
                Row->SetStringField(TEXT("mesh"),GetPathNameSafe(M->GetStaticMesh()));
                FString Mats;for(int32 i=0;i<M->GetNumMaterials();++i)Mats+=GetPathNameSafe(M->GetMaterial(i))+TEXT(";");
                Row->SetStringField(TEXT("materials"),Mats);
                if(UInstancedStaticMeshComponent* I=Cast<UInstancedStaticMeshComponent>(M))Row->SetNumberField(TEXT("instances"),I->GetInstanceCount());
            }
            Row->SetStringField(TEXT("min"),B.Min.ToString());Row->SetStringField(TEXT("max"),B.Max.ToString());
            Row->SetStringField(TEXT("loc"),C->GetComponentLocation().ToString());
            Row->SetStringField(TEXT("rot"),C->GetComponentRotation().ToString());
            Row->SetStringField(TEXT("scale"),C->GetComponentScale().ToString());
            Row->SetBoolField(TEXT("visible"),C->IsVisible()&&!C->bHiddenInGame);
            Row->SetStringField(TEXT("collision"),UEnum::GetValueAsString(C->GetCollisionEnabled()));
            Row->SetBoolField(TEXT("movable"),C->Mobility==EComponentMobility::Movable);
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        TSharedPtr<FJsonObject> Report=MakeShared<FJsonObject>();Report->SetArrayField(TEXT("components"),Rows);
        FString Json;FJsonSerializer::Serialize(Report.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json,*FPaths::Combine(S.Dir,TEXT("components.json")));
        // Occupancy maps, 20 cm cells (x across, y down rows).
        FCollisionQueryParams Q(SCENE_QUERY_STAT(BlackHallSurvey),true,Pawn);
        const float Cell=20.f;
        for(float Z:{-300.f,-230.f,-120.f,-70.f})
        {
            FString Map=FString::Printf(TEXT("z=%.0f x from %.0f step %.0f (columns), y from %.0f (rows)\n"),Z,HallSurveyRegion.Min.X,Cell,HallSurveyRegion.Min.Y);
            for(float Y=HallSurveyRegion.Min.Y;Y<=HallSurveyRegion.Max.Y;Y+=Cell)
            {
                for(float X=HallSurveyRegion.Min.X;X<=HallSurveyRegion.Max.X;X+=Cell)
                {
                    const bool bHit=World->OverlapBlockingTestByChannel(FVector(X,Y,Z),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeBox(FVector(Cell*.45f,Cell*.45f,6.f)),Q);
                    Map+=bHit?TEXT("#"):TEXT(".");
                }
                Map+=TEXT("\n");
            }
            FFileHelper::SaveStringToFile(Map,*FPaths::Combine(S.Dir,FString::Printf(TEXT("occupancy_z%.0f.txt"),-Z)));
        }
        // Floor / ceiling heights on a 50 cm grid.
        FString Heights=TEXT("x,y,floor,ceiling\n");
        for(float Y=HallSurveyRegion.Min.Y;Y<=HallSurveyRegion.Max.Y;Y+=50.f)for(float X=HallSurveyRegion.Min.X;X<=HallSurveyRegion.Max.X;X+=50.f)
        {
            FHitResult Down,Up;
            const bool bD=World->LineTraceSingleByChannel(Down,FVector(X,Y,-200),FVector(X,Y,-450),ECC_Visibility,Q);
            const bool bU=World->LineTraceSingleByChannel(Up,FVector(X,Y,-200),FVector(X,Y,80),ECC_Visibility,Q);
            Heights+=FString::Printf(TEXT("%.0f,%.0f,%.1f,%.1f\n"),X,Y,bD?Down.ImpactPoint.Z:-999.f,bU?Up.ImpactPoint.Z:999.f);
        }
        FFileHelper::SaveStringToFile(Heights,*FPaths::Combine(S.Dir,TEXT("heights.csv")));
        // Views: lattice inside free space at eye height.
        for(float X=HallSurveyRegion.Min.X+200;X<HallSurveyRegion.Max.X;X+=400)for(float Y=HallSurveyRegion.Min.Y+200;Y<HallSurveyRegion.Max.Y;Y+=400)
        {
            const FVector P(X,Y,-170);
            if(World->OverlapBlockingTestByChannel(P,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(30),Q))continue;
            FHitResult F;if(!World->LineTraceSingleByChannel(F,P,P-FVector(0,0,260),ECC_Visibility,Q) || F.ImpactPoint.Z>-280 || F.ImpactPoint.Z<-360)continue;
            for(int32 Yaw=0;Yaw<360;Yaw+=90){S.Views.Add(P);S.Rots.Add(FRotator(-8,Yaw,0));S.Names.Add(FString::Printf(TEXT("v_%04.0f_%04.0f_y%03d"),X,Y,Yaw));}
        }
        UE_LOG(LogTemp,Display,TEXT("BLACKHALL_SURVEY components=%d views=%d"),Rows.Num(),S.Views.Num());
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);Pawn->SetActorEnableCollision(false);
        S.StepStarted=Now;return;
    }
    if(S.Index>=S.Views.Num() || S.Index>=160 || Now-S.Started>400.){ if(FScreenshotRequest::IsScreenshotRequested())return; S.Index=-1;FPlatformMisc::RequestExitWithStatus(false,0);return;}
    if(S.Placed!=S.Index)
    {
        if(FScreenshotRequest::IsScreenshotRequested())return;
        S.Placed=S.Index;S.StepStarted=Now;
        Pawn->SetActorLocation(S.Views[S.Index]-FVector(0,0,Pawn->BaseEyeHeight),false,nullptr,ETeleportType::TeleportPhysics);
        PC->SetControlRotation(S.Rots[S.Index]);
        return;
    }
    PC->SetControlRotation(S.Rots[S.Index]);
    if(Now-S.StepStarted>0.9f && !FScreenshotRequest::IsScreenshotRequested())
    {
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir,S.Names[S.Index]+TEXT(".png")),false,false);
        ++S.Index;
    }
}

namespace
{
struct FHallShot { FString Name; FVector Pos; FRotator Rot; float Fov; };
struct FHallReviewState
{
    TWeakObjectPtr<UWorld> World; TWeakObjectPtr<ACameraActor> Cam;
    int32 Step = 0, Shot = 0, Placed = -1; double StepStarted = 0, Started = 0; FString Dir;
    TSharedPtr<FJsonObject> Report; TArray<FHallShot> Shots;
    float WalkStartY = 0;
    TWeakObjectPtr<AHouseBlackHallLitter> KickTarget; FVector KickStart = FVector::ZeroVector;
};
AHouseBlackHallDoor* FindHallDoor(UWorld* World, const TCHAR* Id)
{
    for (TActorIterator<AHouseBlackHallDoor> It(World); It; ++It) if (It->DoorId == FName(Id)) return *It;
    return nullptr;
}
bool HallSweepClear(UWorld* World, APawn* Pawn, const FVector& A, const FVector& B, FString* HitName)
{
    FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(HallReviewSweep), false, Pawn);
    const bool bHit = World->SweepSingleByChannel(Hit, A, B, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(30.f, 80.f), Q);
    if (bHit && HitName) *HitName = GetNameSafe(Hit.GetComponent()) + TEXT("@") + GetNameSafe(Hit.GetActor()) + FString::Printf(TEXT(" at %s"), *Hit.ImpactPoint.ToCompactString());
    return !bHit;
}
}

void TickHouseBlackHallReview(AStoryFirstPersonCharacter* Pawn)
{
    if (!Pawn->HasAuthority() || !Pawn->IsLocallyControlled()) return;
    UWorld* World = Pawn->GetWorld();
    if (!World || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
    if (!PC) return;
    static FHallReviewState S;
    const double Now = World->GetTimeSeconds();
    if (S.World.Get() != World)
    {
        S = FHallReviewState(); S.World = World; S.Started = S.StepStarted = Now;
        S.Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HouseBlackHallReview")));
        IFileManager::Get().MakeDirectory(*S.Dir, true);
        S.Report = MakeShared<FJsonObject>();
        S.Shots = {
            { TEXT("p2_doorwall"), FVector(60, -380, -160), FRotator(3, 90, 0), 104 },
            { TEXT("p1_exitdoor"), FVector(860, 300, -165), FRotator(2, 10, 0), 88 },
            { TEXT("p4_overview"), FVector(-1080, -250, -158), FRotator(-3, 4, 0), 100 },
            { TEXT("p6_stage"), FVector(-250, 260, -160), FRotator(-2, -100, 0), 100 },
            { TEXT("p3_mural"), FVector(-640, -130, -165), FRotator(4, 180, 0), 90 },
            { TEXT("pair2_close"), FVector(0, 470, -170), FRotator(2, 90, 0), 80 },
            { TEXT("xxvii_close"), FVector(980, 70, -185), FRotator(2, 0, 0), 78 },
            { TEXT("floor_tape"), FVector(60, -60, -80), FRotator(-58, 75, 0), 100 },
            { TEXT("north_east_corner"), FVector(700, 200, -160), FRotator(2, 50, 0), 95 },
        };
    }
    if (S.Step < 0) return;
    const float Age = Now - S.StepStarted;
    auto Next = [&](int32 N) { S.Step = N; S.StepStarted = Now; };
    auto Shoot = [&](const FString& Name) { FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir, Name + TEXT(".png")), false, false); };
    auto View = [&](const FVector& P, const FRotator& R, float Fov)
    {
        if (ACameraActor* C = S.Cam.Get())
        {
            C->SetActorLocationAndRotation(P, R);
            C->GetCameraComponent()->SetFieldOfView(Fov);
            if (PC->GetViewTarget() != C) PC->SetViewTarget(C);
        }
    };
    if (S.Step == 0)
    {
        if (Age < 5.f) return;
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        S.Cam = World->SpawnActor<ACameraActor>(FVector::ZeroVector, FRotator::ZeroRotator, P);
        if (S.Cam.IsValid())
        {
            UCameraComponent* CC = S.Cam->GetCameraComponent();
            CC->bConstrainAspectRatio = false;
            if (Pawn->FirstPersonCamera)
            {
                CC->PostProcessSettings = Pawn->FirstPersonCamera->PostProcessSettings;
                CC->PostProcessBlendWeight = Pawn->FirstPersonCamera->PostProcessBlendWeight;
            }
        }
        Pawn->SetActorHiddenInGame(true);
        { TArray<AActor*> Attached; Pawn->GetAttachedActors(Attached, true, true); for (AActor* A : Attached) if (A) A->SetActorHiddenInGame(true); }
        if (PC->MyHUD) PC->MyHUD->bShowHUD = false;
        if (UHouseBlackHallSubsystem* Sub = World->GetSubsystem<UHouseBlackHallSubsystem>())
        {
            if (AHouseBlackHallDetail* D = Sub->GetDetail())
            {
                S.Report->SetBoolField(TEXT("rebuilt"), D->bRebuilt);
                S.Report->SetNumberField(TEXT("replaced_tiles"), D->ReplacedTiles);
                S.Report->SetNumberField(TEXT("clipped_collision"), D->ClippedCollision);
                S.Report->SetNumberField(TEXT("removed_triangles"), D->RemovedTriangles);
                S.Report->SetNumberField(TEXT("built_triangles"), D->BuiltTriangles);
                S.Report->SetStringField(TEXT("notes"), D->Notes);
            }
            S.Report->SetNumberField(TEXT("doors"), Sub->GetDoors().Num());
        }
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying); Pawn->SetActorEnableCollision(false);
        Pawn->SetActorLocation(FVector(0, -600, -200), false, nullptr, ETeleportType::TeleportPhysics);
        Next(1); return;
    }
    if (S.Step == 1)
    {
        if (S.Shot >= S.Shots.Num()) { Next(10); return; }
        const FHallShot& Sh = S.Shots[S.Shot];
        if (S.Placed != S.Shot) { if (FScreenshotRequest::IsScreenshotRequested()) return; S.Placed = S.Shot; S.StepStarted = Now; View(Sh.Pos, Sh.Rot, Sh.Fov); return; }
        if (Age > 1.2f && !FScreenshotRequest::IsScreenshotRequested()) { Shoot(Sh.Name); ++S.Shot; }
        return;
    }
    const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (S.Step == 10)
    {
        if (FScreenshotRequest::IsScreenshotRequested()) return;
        // Closed patio pair must block the player.
        FString Hit;
        S.Report->SetBoolField(TEXT("pair2_closed_clear"), HallSweepClear(World, Pawn, FVector(0, 700, HallSpec::ZF + Half + 2), FVector(0, 950, HallSpec::ZF + Half + 2), &Hit));
        S.Report->SetStringField(TEXT("pair2_closed_hit"), Hit);
        Pawn->SetActorEnableCollision(true); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Pawn->SetActorLocation(FVector(0, 640, HallSpec::ZF + Half + 2), false, nullptr, ETeleportType::TeleportPhysics);
        AHouseBlackHallDoor* Door = FindHallDoor(World, TEXT("BH_Patio2_A"));
        S.Report->SetBoolField(TEXT("pair2_found"), Door != nullptr);
        S.Report->SetBoolField(TEXT("pair2_interact"), Door && Door->Interact(Pawn));
        Next(11); return;
    }
    if (S.Step == 11)
    {
        if (Age < 1.5f) return;
        AHouseBlackHallDoor* A = FindHallDoor(World, TEXT("BH_Patio2_A")); AHouseBlackHallDoor* B = FindHallDoor(World, TEXT("BH_Patio2_B"));
        S.Report->SetBoolField(TEXT("pair2_both_open"), A && B && A->IsOpen() && B->IsOpen());
        FString Hit;
        S.Report->SetBoolField(TEXT("pair2_open_clear"), HallSweepClear(World, Pawn, FVector(0, 700, HallSpec::ZF + Half + 2), FVector(0, 1000, HallSpec::ZF + Half + 2), &Hit));
        S.Report->SetStringField(TEXT("pair2_open_hit"), Hit);
        View(FVector(0, 380, -165), FRotator(0, 90, 0), 90);
        Next(12); return;
    }
    if (S.Step == 12)
    {
        if (Age > 1.f && !FScreenshotRequest::IsScreenshotRequested()) { Shoot(TEXT("pair2_open")); S.WalkStartY = Pawn->GetActorLocation().Y; Next(13); }
        return;
    }
    if (S.Step == 13)
    {
        // Walk out through the opened pair onto the landing.
        if (FScreenshotRequest::IsScreenshotRequested()) return;
        Pawn->AddMovementInput(FVector(0, 1, 0), 1.f);
        if (Age < 3.f) return;
        S.Report->SetNumberField(TEXT("walk_out_start_y"), S.WalkStartY);
        S.Report->SetNumberField(TEXT("walk_out_end_y"), Pawn->GetActorLocation().Y);
        S.Report->SetBoolField(TEXT("walked_outside"), Pawn->GetActorLocation().Y > 860.f);
        S.Report->SetNumberField(TEXT("walk_out_end_z"), Pawn->GetActorLocation().Z);
        View(FVector(0, 1040, -165), FRotator(0, -90, 0), 95);
        Next(14); return;
    }
    if (S.Step == 14)
    {
        if (Age > 1.f && !FScreenshotRequest::IsScreenshotRequested()) { Shoot(TEXT("outside_look_in")); Next(15); }
        return;
    }
    if (S.Step == 15)
    {
        if (FScreenshotRequest::IsScreenshotRequested()) return;
        Pawn->SetActorLocation(FVector(1120, 490, HallSpec::ZF + Half + 2), false, nullptr, ETeleportType::TeleportPhysics);
        FString Hit;
        S.Report->SetBoolField(TEXT("exit_closed_clear"), HallSweepClear(World, Pawn, FVector(1120, 490, HallSpec::ZF + Half + 2), FVector(1420, 490, HallSpec::ZF + Half + 2), &Hit));
        S.Report->SetStringField(TEXT("exit_closed_hit"), Hit);
        AHouseBlackHallDoor* Exit = FindHallDoor(World, TEXT("BH_ExitDoor"));
        S.Report->SetBoolField(TEXT("exit_interact"), Exit && Exit->Interact(Pawn));
        AHouseBlackHallDoor* Equip = FindHallDoor(World, TEXT("BH_EquipmentDoor"));
        S.Report->SetBoolField(TEXT("equipment_locked"), Equip && Equip->bLocked && !Equip->Interact(Pawn));
        Next(16); return;
    }
    if (S.Step == 16)
    {
        if (Age < 1.5f) return;
        FString Hit;
        S.Report->SetBoolField(TEXT("exit_open_clear"), HallSweepClear(World, Pawn, FVector(1120, 490, HallSpec::ZF + Half + 2), FVector(1420, 490, HallSpec::ZF + Half + 2), &Hit));
        S.Report->SetStringField(TEXT("exit_open_hit"), Hit);
        View(FVector(980, 330, -165), FRotator(0, 25, 0), 90);
        Next(17); return;
    }
    if (S.Step == 17)
    {
        if (Age > 1.f && !FScreenshotRequest::IsScreenshotRequested()) { Shoot(TEXT("exit_open")); Next(18); }
        return;
    }
    if (S.Step == 18)
    {
        // Litter: count, pick a can near the middle for the kick test, and capture close views.
        if (FScreenshotRequest::IsScreenshotRequested() || Age < .5f) return;
        int32 Count = 0; AHouseBlackHallLitter* Best = nullptr; float BestD = BIG_NUMBER;
        for (TActorIterator<AHouseBlackHallLitter> It(World); It; ++It)
        {
            ++Count;
            const float D = FVector::Dist2D(It->GetActorLocation(), FVector(0, 100, 0));
            if (It->Kind == AHouseBlackHallLitter::Can && D < BestD && It->GetActorLocation().Z < HallSpec::ZF + 20.f) { BestD = D; Best = *It; }
        }
        S.Report->SetNumberField(TEXT("litter_count"), Count);
        S.KickTarget = Best;
        if (Best)
        {
            S.KickStart = Best->GetActorLocation();
            const FVector From = S.KickStart + FVector(-170, 0, 0);
            Pawn->SetActorLocation(FVector(From.X, From.Y, HallSpec::ZF + Half + 2), false, nullptr, ETeleportType::TeleportPhysics);
            PC->SetControlRotation(FRotator(0, 0, 0));
        }
        Next(19); return;
    }
    if (S.Step == 19)
    {
        if (Age < 1.8f) { Pawn->AddMovementInput(FVector(1, 0, 0), 1.f); return; }
        if (Age < 3.3f) return;
        if (AHouseBlackHallLitter* T = S.KickTarget.Get())
        {
            S.Report->SetNumberField(TEXT("kick_moved_cm"), FVector::Dist(T->GetActorLocation(), S.KickStart));
            S.Report->SetBoolField(TEXT("kick_moved"), FVector::Dist(T->GetActorLocation(), S.KickStart) > 25.f);
        }
        Pawn->SetActorLocation(FVector(0, -600, HallSpec::ZF + Half + 2), false, nullptr, ETeleportType::TeleportPhysics);
        Next(20); return;
    }
    if (S.Step >= 20 && S.Step < 27)
    {
        struct FLook { const TCHAR* Name; FVector From, To; };
        const FLook Looks[7] = {
            { TEXT("keg_close"), FVector(520, 330, -235), FVector(645, 460, HallSpec::ZF + 45) },
            { TEXT("litter_stage"), FVector(-260, -120, -205), FVector(-260, -440, HallSpec::ZF) },
            { TEXT("litter_west"), FVector(-880, -60, -215), FVector(-1160, -120, HallSpec::ZF) },
            { TEXT("litter_overview"), FVector(420, 380, -140), FVector(-150, -250, HallSpec::ZF) },
            { TEXT("wear_north"), FVector(1010, 470, -225), FVector(1100, 792, HallSpec::ZF + 95) },
            { TEXT("wear_south"), FVector(-1010, -470, -225), FVector(-1100, -792, HallSpec::ZF + 95) },
            { TEXT("wear_west"), FVector(-960, -380, -215), FVector(-1224, -560, HallSpec::ZF + 95) },
        };
        const FLook& L = Looks[S.Step - 20];
        if (S.Placed != 100 + S.Step) { if (FScreenshotRequest::IsScreenshotRequested()) return; S.Placed = 100 + S.Step; S.StepStarted = Now; View(L.From, (L.To - L.From).Rotation(), 90); return; }
        if (Age > 1.2f && !FScreenshotRequest::IsScreenshotRequested()) { Shoot(L.Name); Next(S.Step + 1); }
        return;
    }
    if (S.Step == 27)
    {
        if (FScreenshotRequest::IsScreenshotRequested() || Age < 1.f) return;
        S.Report->SetNumberField(TEXT("seconds"), Now - S.Started);
        FString Json; FJsonSerializer::Serialize(S.Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json, *FPaths::Combine(S.Dir, TEXT("result.json")));
        UE_LOG(LogTemp, Display, TEXT("BLACKHALL_REVIEW_DONE %s"), *Json);
        S.Step = -1;
        FPlatformMisc::RequestExitWithStatus(false, 0);
    }
}
#else
void TickHouseBlackHallSurvey(AStoryFirstPersonCharacter*){}
void TickHouseBlackHallReview(AStoryFirstPersonCharacter*){}
#endif
