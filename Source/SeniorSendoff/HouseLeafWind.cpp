#include "HouseLeafWind.h"
#include "HouseExteriorNature.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallMesh.h"
#include "HouseDetail.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundAttenuation.h"

#if !UE_BUILD_SHIPPING
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "HighResScreenshot.h"
#include "Kismet/KismetMathLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#endif

namespace
{
constexpr int32 LeafCount = 96;
constexpr float PoolRadius = 1450.f;
float GustAt(double Time)
{
    const float Main = FMath::Max(0.f,FMath::Sin(float(Time)*.42f-1.f));
    const float Small = FMath::Max(0.f,FMath::Sin(float(Time)*.79f+.3f));
    return FMath::Clamp(.13f + .82f*Main*Main + .17f*Small*Small*Small,0.f,1.f);
}
}

AHouseLeafWind::AHouseLeafWind()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    bReplicates = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("LeafWindOrigin"));
    for(int32 K=0;K<3;++K)
    {
        UInstancedStaticMeshComponent* P=CreateDefaultSubobject<UInstancedStaticMeshComponent>(
            *FString::Printf(TEXT("DriftingLeaves%d"),K));
        P->SetupAttachment(RootComponent);
        P->SetMobility(EComponentMobility::Movable);
        P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        P->SetGenerateOverlapEvents(false);
        P->SetCanEverAffectNavigation(false);
        P->SetCastShadow(false);
        P->SetCullDistances(1150,1750);
        Pools.Add(P);
    }
    Rustle=CreateDefaultSubobject<UAudioComponent>(TEXT("LocalLeafRustle"));
    Rustle->SetupAttachment(RootComponent);
    Rustle->bAutoActivate=false;
    Rustle->bAutoDestroy=false;
    Rustle->bAllowSpatialization=true;
    Rustle->bOverrideAttenuation=true;
    FSoundAttenuationSettings& A=Rustle->AttenuationOverrides;
    A.bAttenuate=true;
    A.bSpatialize=true;
    A.AttenuationShape=EAttenuationShape::Sphere;
    A.AttenuationShapeExtents=FVector(150.f);
    A.FalloffDistance=1000.f;
    A.bEnableOcclusion=true;
    A.OcclusionTraceChannel=ECC_Visibility;
    A.OcclusionLowPassFilterFrequency=800.f;
    A.OcclusionVolumeAttenuation=.08f;
    A.OcclusionInterpolationTime=.35f;
    Rustle->SetVolumeMultiplier(0.f);
}

void AHouseLeafWind::Initialize(UHouseBlackHallSubsystem* Look,const TArray<FTransform>& FallenLeaves)
{
    if(!Look||!GetWorld()||GetNetMode()==NM_DedicatedServer)
    {SetActorTickEnabled(false);return;}
    for(int32 K=0;K<3;++K)
    {
        FHallMesh M;HouseNature::SingleLeaf(M,K);
        UStaticMesh* Mesh=HwBuildStaticMesh(M,*FString::Printf(TEXT("SM_WindLeaf%d"),K),Look,{});
        if(!Mesh){SetActorTickEnabled(false);return;}
        Pools[K]->SetStaticMesh(Mesh);
    }
    for(int32 K=0;K<LeafCount;++K)
    {
        FHouseWindLeaf L;
        L.Palette=K%3;
        L.Instance=Pools[L.Palette]->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector(.0001f)));
        L.Size=Random.FRandRange(.72f,1.35f);
        L.Phase=Random.FRandRange(0.f,2.f*PI);
        L.Threshold=Random.FRandRange(.24f,.58f);
        L.Drag=Random.FRandRange(.7f,1.35f);
        Leaves.Add(L);
    }
    for(const FTransform& T:FallenLeaves)Seeds.Add(T.GetTranslation());
    const FName Names[]={TEXT("EX_NatureGold"),TEXT("EX_NatureCopper"),TEXT("EX_NatureRust"),
        TEXT("EX_NatureGreenLeaf"),TEXT("EX_NatureShrub"),TEXT("EX_NatureShrubTips"),
        TEXT("EX_NatureGrass"),TEXT("EX_NatureDryGrass")};
    for(int32 K=0;K<8;++K)if(UMaterialInstanceDynamic* M=Cast<UMaterialInstanceDynamic>(Look->Mat(Names[K])))
    {
        WindMaterials.Add(M);
        WindAmplitudes.Add(K<4?2.4f:K==4?.6f:K==5?.9f:K==6?2.7f:3.8f);
    }
    RustleWave=LoadObject<USoundWave>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/S_LeavesRustlingLoop.S_LeavesRustlingLoop"));
    if(RustleWave)
    {
        RustleWave->bLooping=true;
        Rustle->SetSound(RustleWave);
        Rustle->OnAudioPlaybackPercentNative.AddUObject(this,&AHouseLeafWind::AudioPlayback);
    }
    else UE_LOG(LogTemp,Error,TEXT("LEAF_WIND missing saved rustle sound"));
    bReady=true;
    UE_LOG(LogTemp,Display,TEXT("LEAF_WIND ready pool=%d palettes=3 seed_patches=%d audio=%s; local cosmetic/no collision/no added lights"),
        Leaves.Num(),Seeds.Num(),*GetNameSafe(RustleWave));
}

bool AHouseLeafWind::FindGround(const FVector& Point,FVector& Ground,FVector& Normal) const
{
    if(!GetWorld()||FMath::Abs(Point.X)>8250||Point.Y < -6100||Point.Y>6000
        ||!HouseNature::IsLeafGround(Point.X,Point.Y))return false;
    const float Expected=HouseNature::ApproximateGroundHeight(Point.X,Point.Y);
    FHitResult Hit;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(LeafWindGround),true,this);
    if(APlayerController* P=GetWorld()->GetFirstPlayerController())if(P->GetPawn())Q.AddIgnoredActor(P->GetPawn());
    if(!GetWorld()->LineTraceSingleByChannel(Hit,FVector(Point.X,Point.Y,Expected+82.f),
        FVector(Point.X,Point.Y,Expected-125.f),ECC_Visibility,Q))return false;
    if(Hit.ImpactNormal.Z<.65f||Hit.ImpactPoint.Z>Expected+72.f)return false;
    Ground=Hit.ImpactPoint;
    Normal=Hit.ImpactNormal;
    return true;
}

bool AHouseLeafWind::RecycleLeaf(FHouseWindLeaf& L,const FVector& Viewer,const FVector& Forward)
{
    for(int32 Attempt=0;Attempt<18;++Attempt)
    {
        const float A=Random.FRandRange(0.f,2.f*PI),D=Random.FRandRange(220.f,1000.f);
        FVector P=Viewer+Forward*250.f+FVector(FMath::Cos(A)*D,FMath::Sin(A)*D,0);
        // Some leaves originate in existing piles; the rest pass across open lawn and walks.
        if(Attempt<6 && Seeds.Num()>0)
        {
            const FVector Seed=Seeds[Random.RandRange(0,Seeds.Num()-1)];
            if(FVector::DistSquared2D(Seed,Viewer)<FMath::Square(1050.f))P=Seed+FVector(Random.FRandRange(-40,40),Random.FRandRange(-40,40),0);
        }
        FVector Ground,Normal;
        if(!FindGround(P,Ground,Normal))continue;
        L.Position=Ground;L.GroundZ=Ground.Z;L.Normal=Normal;
        L.LastGroundQuery=Ground;L.Age=0.f;
        L.Yaw=Random.FRandRange(-180.f,180.f);L.Roll=L.Pitch=0.f;
        L.Velocity=FVector::ZeroVector;
        return true;
    }
    L.Age=-1.f;
    return false;
}

void AHouseLeafWind::AudioPlayback(const UAudioComponent* Component,const USoundWave* Wave,float Percent)
{
    if(Component==Rustle.Get()&&Wave==RustleWave.Get()&&FMath::IsFinite(Percent))++AudioSamples;
}

void AHouseLeafWind::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!bReady||!GetWorld())return;
    APlayerController* Player=GetWorld()->GetFirstPlayerController();
    if(!Player||!Player->IsLocalController()||!Player->PlayerCameraManager)return;
    const double WorldTime=GetWorld()->GetTimeSeconds();
    Review(Player,WorldTime);
    const double Time=GetWorld()->GetGameState()?GetWorld()->GetGameState()->GetServerWorldTimeSeconds():WorldTime;
    Gust=GustAt(Time);
    for(int32 K=0;K<WindMaterials.Num();++K)
        WindMaterials[K]->SetScalarParameterValue(TEXT("WindStrength"),WindAmplitudes[K]*(.25f+Gust*2.1f));
    const FVector Viewer=Player->PlayerCameraManager->GetCameraLocation();
    FVector Forward=Player->PlayerCameraManager->GetCameraRotation().Vector();
    Forward.Z=0;Forward.Normalize();
    const float Expected=HouseNature::ApproximateGroundHeight(Viewer.X,Viewer.Y);
    const bool Outside=HouseNature::IsLeafGround(Viewer.X,Viewer.Y)&&Viewer.Z>Expected-35.f&&Viewer.Z<Expected+650.f;
    const float Dt=FMath::Min(DeltaSeconds,.05f);
    ListenerGain=FMath::FInterpTo(ListenerGain,Outside?1.f:0.f,Dt,3.5f);
    const float Direction=-.37f+FMath::Sin(float(Time)*.077f)*.36f;
    const FVector Wind(FMath::Cos(Direction),FMath::Sin(Direction),0),Across(-Wind.Y,Wind.X,0);
    MovingLeaves=0;GroundChecks=0;
    FVector SoundPosition=Viewer;float Nearest=FLT_MAX;
    if(!Outside&&bLeavesVisible)
    {
        for(FHouseWindLeaf& L:Leaves)
        {
            L.Age=-1.f;
            Pools[L.Palette]->UpdateInstanceTransform(L.Instance,FTransform(FQuat::Identity,L.Position,FVector(.0001f)),true,false,true);
        }
        for(UInstancedStaticMeshComponent* P:Pools)P->MarkRenderStateDirty();
        bLeavesVisible=false;
    }
    if(Outside)for(FHouseWindLeaf& L:Leaves)
    {
        if(L.Age<0 || L.Age>48.f || FVector::DistSquared2D(L.Position,Viewer)>FMath::Square(PoolRadius))
            if(!RecycleLeaf(L,Viewer,Forward))
            {
                Pools[L.Palette]->UpdateInstanceTransform(L.Instance,FTransform(FQuat::Identity,L.Position,FVector(.0001f)),true,false,true);
                continue;
            }
        L.Age+=Dt;
        const float Push=FMath::Max(0.f,Gust-L.Threshold);
        const FVector Target=Wind*(Push*165.f*L.Drag)
            +Across*(FMath::Sin(float(Time)*1.7f+L.Phase)*Push*22.f);
        L.Velocity=FMath::VInterpTo(L.Velocity,Target,Dt,Push>.02f?2.3f:5.f);
        FVector Next=L.Position+L.Velocity*Dt;
        bool Clear=HouseNature::IsLeafGround(Next.X,Next.Y);
        if(Clear&&GroundChecks<10&&FVector::DistSquared2D(Next,L.LastGroundQuery)>144.f)
        {
            ++GroundChecks;
            FVector Ground,Normal;
            Clear=FindGround(Next,Ground,Normal)&&FMath::Abs(Ground.Z-L.GroundZ)<16.f;
            if(Clear)
            {
                FHitResult Block;
                FCollisionQueryParams Q(SCENE_QUERY_STAT(LeafWindObstacle),true,this);
                Clear=!GetWorld()->LineTraceSingleByChannel(Block,L.LastGroundQuery+FVector(0,0,10),Ground+FVector(0,0,10),ECC_Visibility,Q);
                if(Clear){L.GroundZ=Ground.Z;L.Normal=Normal;L.LastGroundQuery=Ground;}
            }
        }
        if(Clear)L.Position=Next;else L.Velocity*=.12f;
        L.Position.Z=FMath::FInterpTo(L.Position.Z,L.GroundZ,Dt,12.f);
        const float Speed=L.Velocity.Size2D();
        if(Speed>8.f)++MovingLeaves;
        const float Tumble=FMath::SmoothStep(12.f,65.f,Speed);
        L.Phase+=Speed*Dt/(26.f*L.Size);
        L.Roll=FMath::FInterpTo(L.Roll,FMath::Sin(L.Phase)*58.f*Tumble,Dt,7.f);
        L.Pitch=FMath::FInterpTo(L.Pitch,FMath::Sin(L.Phase*1.27f+.8f)*25.f*Tumble,Dt,7.f);
        if(Speed>3.f)L.Yaw=FMath::FixedTurn(L.Yaw,L.Velocity.Rotation().Yaw+FMath::Sin(L.Phase)*28.f,Dt*80.f);
        const float Lift=.7f+FMath::Abs(FMath::Sin(L.Phase))*4.f*Tumble
            +FMath::Max(0.f,FMath::Sin(L.Phase*1.7f))*5.f*Push;
        const float Fade=FMath::SmoothStep(0.f,1.1f,L.Age);
        const FQuat GroundRotation=FRotationMatrix::MakeFromZ(L.Normal).ToQuat();
        const FQuat Rotation=GroundRotation*FRotator(L.Pitch,L.Yaw,L.Roll).Quaternion();
        Pools[L.Palette]->UpdateInstanceTransform(L.Instance,
            FTransform(Rotation,L.Position+FVector(0,0,Lift),FVector(L.Size*Fade)),true,false,true);
        const float Distance=FVector::DistSquared(L.Position,Viewer);
        if(Distance<Nearest){Nearest=Distance;SoundPosition=L.Position+FVector(0,0,18);}
    }
    if(Outside)
    {
        for(UInstancedStaticMeshComponent* P:Pools)P->MarkRenderStateDirty();
        bLeavesVisible=true;
        LastNearby=Nearest==FLT_MAX?0.f:1.f-FMath::SmoothStep(700.f,1150.f,FMath::Sqrt(Nearest));
        Rustle->SetWorldLocation(SoundPosition);
    }
    RustleVolume=ListenerGain*LastNearby*(.055f+Gust*.25f);
    Rustle->SetVolumeMultiplier(RustleVolume);
    if(RustleWave&&!bSoundStarted&&Outside&&LastNearby>.1f)
    {
        bSoundStarted=true;
        Rustle->Play(Random.FRandRange(0.f,5.f));
    }
}

void AHouseLeafWind::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Rustle)Rustle->Stop();
    Super::EndPlay(Reason);
}

void AHouseLeafWind::ReviewView(APlayerController* Player,const FVector& From,const FVector& To)
{
#if !UE_BUILD_SHIPPING
    if(!ReviewCamera)
    {
        ReviewCamera=GetWorld()->SpawnActor<ACameraActor>();
        ReviewCamera->GetCameraComponent()->SetFieldOfView(78.f);
    }
    ReviewCamera->SetActorLocation(From);
    ReviewCamera->SetActorRotation(UKismetMathLibrary::FindLookAtRotation(From,To));
    Player->SetViewTarget(ReviewCamera);
#endif
}

void AHouseLeafWind::Review(APlayerController* Player,double Now)
{
#if !UE_BUILD_SHIPPING
    static const bool Enabled=FParse::Param(FCommandLine::Get(),TEXT("HouseLeafWindReview"));
    if(!Enabled)return;
    const FString Dir=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("LeafWindReview"));
    if(ReviewStart<0.)
    {
        ReviewStart=Now;IFileManager::Get().MakeDirectory(*Dir,true);
        ReviewView(Player,FVector(750,-3400,58),FVector(1100,-2750,-38));
        UE_LOG(LogTemp,Display,TEXT("LEAF_WIND_REVIEW started: normal shared gusts, actual ground queries and native audio cursor"));
    }
    const float Age=Now-ReviewStart;
    if(Age>2.f&&Age<18.f)
    {
        ReviewMinGust=FMath::Min(ReviewMinGust,Gust);ReviewMaxGust=FMath::Max(ReviewMaxGust,Gust);
        ReviewOutsideVolume=FMath::Max(ReviewOutsideVolume,RustleVolume);
        if(ReviewPositions.Num()!=Leaves.Num())for(const FHouseWindLeaf& L:Leaves)ReviewPositions.Add(L.Position);
        else for(int32 K=0;K<Leaves.Num();++K)
            ReviewTravel=FMath::Max(ReviewTravel,float(FVector::Dist2D(Leaves[K].Position,ReviewPositions[K])));
    }
    auto Shot=[&](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::Combine(Dir,Name),false,false);};
    if(ReviewStage==0&&Age>4.f){Shot(TEXT("LeavesBeforeGust.png"));ReviewStage=1;}
    if(ReviewStage==1&&Age>9.f){Shot(TEXT("LeavesTumbling.png"));ReviewStage=2;}
    if(ReviewStage==2&&Age>17.f){Shot(TEXT("LeavesSettling.png"));ReviewStage=3;}
    if(ReviewStage==3&&Age>18.f)
    {ReviewView(Player,FVector(-35,-360,150),FVector(150,-100,120));ReviewStage=4;}
    if(ReviewStage==4&&Age>21.5f)
    {
        ReviewInsideVolume=RustleVolume;
        UE_LOG(LogTemp,Display,TEXT("LEAF_WIND_REVIEW indoors gain=%.5f native_audio_samples=%d"),RustleVolume,AudioSamples);
        ReviewView(Player,FVector(-36,-2640,85),FVector(80,-2310,-35));ReviewStage=5;
    }
    if(Now-ReviewLastReport>3.f)
    {
        ReviewLastReport=Now;
        UE_LOG(LogTemp,Display,TEXT("LEAF_WIND_REVIEW age=%.1f gust=%.2f moving=%d gain=%.3f native_audio_samples=%d"),Age,Gust,MovingLeaves,RustleVolume,AudioSamples);
    }
    if(ReviewStage==5&&Age>25.f)
    {
        Shot(TEXT("LeavesOnFrontWalk.png"));ReviewStage=6;
        const bool Passed=Leaves.Num()==LeafCount&&ReviewTravel>30.f&&ReviewMaxGust>.8f&&ReviewMinGust<.3f
            &&ReviewOutsideVolume>.1f&&ReviewInsideVolume<.005f&&RustleVolume>.02f&&AudioSamples>20&&Rustle->IsPlaying();
        const FString Report=FString::Printf(TEXT("%s: pool=%d drift_cm=%.1f gust_range=%.2f..%.2f outdoor_volume=%.3f indoor_volume=%.5f reentry_volume=%.3f native_audio_samples=%d playing=%d\n"),
            Passed?TEXT("PASS"):TEXT("FAIL"),Leaves.Num(),ReviewTravel,ReviewMinGust,ReviewMaxGust,ReviewOutsideVolume,ReviewInsideVolume,RustleVolume,AudioSamples,Rustle->IsPlaying());
        FFileHelper::SaveStringToFile(Report,*FPaths::Combine(Dir,TEXT("result.txt")));
        UE_LOG(LogTemp,Display,TEXT("LEAF_WIND_REVIEW %s"),*Report);
    }
    if(ReviewStage==6&&Age>27.f&&!FScreenshotRequest::IsScreenshotRequested())
    {
        FString Result;FFileHelper::LoadFileToString(Result,*FPaths::Combine(Dir,TEXT("result.txt")));
        FPlatformMisc::RequestExitWithStatus(false,Result.StartsWith(TEXT("PASS"))?0:1);
        ReviewStage=7;
    }
#endif
}
