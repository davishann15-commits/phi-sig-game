#include "HouseBasketball.h"
#include "HouseBasketballCourt.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallPaint.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "TimerManager.h"

namespace
{
void BallLook(UHouseBlackHallSubsystem* L)
{
    if(!L || L->Materials.Contains(TEXT("EX_Basketball")))return;
    FHallImage Img;Img.Init(1024,512,FLinearColor::Black);TArray<float> Height;Height.Init(0.f,Img.W*Img.H);
    for(int Y=0;Y<Img.H;++Y)for(int X=0;X<Img.W;++X)
    {
        float U=(X+.5f)/Img.W,V=(Y+.5f)/Img.H;
        float Th=2*PI*U,Ph=PI*V;
        FVector N(FMath::Sin(Ph)*FMath::Cos(Th),FMath::Sin(Ph)*FMath::Sin(Th),FMath::Cos(Ph));
        float Seam=FMath::Min(FMath::Abs(N.Z),FMath::Min(FMath::Abs(N.X),FMath::Min(FMath::Abs((N.X+N.Y)*.7071f),FMath::Abs((N.X-N.Y)*.7071f))));
        float Grain=HallNoise(X*.78f,Y*.78f,114);float Pebble=FMath::Clamp(1.f-FMath::Frac(X/3.f)*1.5f-FMath::Frac(Y/3.f)*1.5f,0.f,1.f);
        bool Dark=Seam<.025f;float Wear=HallFbm(X*.04f,Y*.04f,3,241);
        Img.At(X,Y)=Dark?FLinearColor(.008f,.006f,.004f):FLinearColor(.48f,.135f,.021f)*(.76f+.3f*Grain+.1f*Wear);
        Height[Y*Img.W+X]=Dark?.1f:.7f+.3f*Pebble;
    }
    FHallImage Normal;HallHeightToNormal(Height,Img.W,Img.H,1.25f,true,Normal);
    UTexture2D* T=HallMakeTexture(Img,true,false,TEXT("T_CourtBall")),*N=HallMakeTexture(Normal,false,true,TEXT("T_CourtBallN"));
    L->Textures.Add(TEXT("T_CourtBall"),T);L->Textures.Add(TEXT("T_CourtBallN"),N);
    HallMakeTextured(L,TEXT("EX_Basketball"),T,N,.8f,FLinearColor::White,.8f);
}
}
AHouseBasketball::AHouseBasketball()
{
    IntactMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    PropName=TEXT("basketball");PropId=TEXT("CourtyardBasketball");MassKg=.62f;
    CarrySize=ESSOCarrySize::Small;bMovable=true;bBreakable=false;bPhysicsActive=true;
    // Spawn transform supplies the 24.8 cm diameter. Keep the root unit scaled
    // here so spawning does not multiply the two scales into a tiny ball.
    Body->SetStaticMesh(IntactMesh);
    Body->SetCanEverAffectNavigation(false);SetNetUpdateFrequency(60);SetMinNetUpdateFrequency(20);
    bAlwaysRelevant=true;
}
void AHouseBasketball::BeginPlay()
{
    if(auto* L=GetWorld()->GetSubsystem<UHouseBlackHallSubsystem>())
    {
        BallLook(L);MaterialOverrides={L->Mat(TEXT("EX_Basketball"))};
    }
    Super::BeginPlay();
    Rubber=NewObject<UPhysicalMaterial>(this);Rubber->Friction=.45f;Rubber->StaticFriction=.5f;
    Rubber->Restitution=.76f;Rubber->RestitutionCombineMode=EFrictionCombineMode::Max;Rubber->bOverrideRestitutionCombineMode=true;
    Body->SetPhysMaterialOverride(Rubber);Body->SetUseCCD(true);
    SfxSurface=EHouseSurface::Plastic;Previous=Body->GetComponentLocation();
    // An unbreakable ball needs no furniture damage sampling. This short server timer
    // also continues when Chaos sleeps the body, for lost-ball recovery.
    SetActorTickEnabled(false);
    if(HasAuthority())GetWorldTimerManager().SetTimer(BallTimer,this,&AHouseBasketball::UpdateBall,.025f,true);
}
void AHouseBasketball::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(BallTimer);Super::EndPlay(Reason);
}
float AHouseBasketball::TakeDamage(float Amount,const FDamageEvent& Event,AController* Instigator,AActor* Causer){return 0;}
void AHouseBasketball::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(AHouseBasketball,LeftBaskets);DOREPLIFETIME(AHouseBasketball,RightBaskets);DOREPLIFETIME(AHouseBasketball,LastBasketAt);
}
FVector AHouseBasketball::ShotVelocity(const FVector& View,const FRotator& Aim,const AActor* Shooter)const
{
    FVector From=Body->GetComponentLocation(),Target=FVector::ZeroVector;float Best=.97f;
    bool Court=FMath::Abs(From.X)<1420 && From.Y>1870 && From.Y<3780 && From.Z<HouseCourt::Floor+460;
    if(Court)for(int I=0;I<2;++I)
    {
        FVector Rim=HouseCourt::Rim(I);float Dot=FVector::DotProduct((Rim-View).GetSafeNormal(),Aim.Vector());
        if(Dot<Best || FVector::Dist2D(From,Rim)>2800 || FVector::Dist2D(From,Rim)<70)continue;
        FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(CourtShotSight),false,this);Q.AddIgnoredActor(GetOwner());Q.AddIgnoredActor(Shooter);
        // The carrying pawn can be in the trace near its camera.
        Q.AddIgnoredActor(GetInstigator());
        if(GetWorld()->LineTraceSingleByChannel(Hit,View+Aim.Vector()*40,Rim,ECC_Visibility,Q) && Hit.Distance<FVector::Dist(View+Aim.Vector()*40,Rim)-8)
        {
            if(FParse::Param(FCommandLine::Get(),TEXT("HouseBasketballReview")))UE_LOG(LogTemp,Display,TEXT("COURT_SHOT_BLOCKED actor=%s point=%s rim=%s"),*GetNameSafe(Hit.GetActor()),*Hit.ImpactPoint.ToString(),*Rim.ToString());
            continue;
        }
        Best=Dot;Target=Rim;
    }
    if(Target.IsNearlyZero())
    {
        if(FParse::Param(FCommandLine::Get(),TEXT("HouseBasketballReview")))UE_LOG(LogTemp,Display,TEXT("COURT_SHOT_FREE from=%s view=%s aim=%s court=%d"),*From.ToString(),*View.ToString(),*Aim.ToString(),Court);
        return Aim.Vector()*1050+FVector::UpVector*235;
    }
    float G=FMath::Max(100.f,-GetWorld()->GetGravityZ());float D=FVector::Dist2D(From,Target);
    float Apex=FMath::Max(From.Z,Target.Z)+FMath::Max(140.f,D*.32f);
    float Up=FMath::Sqrt(2*(Apex-From.Z)/G),Down=FMath::Sqrt(2*(Apex-Target.Z)/G),Time=Up+Down;
    // Account for the ball's light air drag and Chaos' semi-implicit gravity
    // step. A continuous parabola alone lands several cm short of the opening.
    const double Drag=.003, Travel=(1.-FMath::Exp(-Drag*Time))/Drag;
    FVector V=(Target-From)/Travel;
    V.Z=((Target.Z-From.Z)+(G/Drag)*Time)/Travel-G/Drag+G*PhysicsStep*.5;
    if(FParse::Param(FCommandLine::Get(),TEXT("HouseBasketballReview")))UE_LOG(LogTemp,Display,TEXT("COURT_SHOT_LOB from=%s target=%s v=%s"),*From.ToString(),*Target.ToString(),*V.ToString());
    return V;
}
void AHouseBasketball::MarkShot(){if(HasAuthority()){ShotAt=GetWorld()->GetTimeSeconds();Previous=Body->GetComponentLocation();}}
void AHouseBasketball::UpdateBall()
{
    if(!HasAuthority()||!Body)return;
    const UPhysicsSettings* Settings=UPhysicsSettings::Get();
    float Step=FMath::Min(GetWorld()->GetDeltaSeconds(),Settings->MaxPhysicsDeltaTime);
    if(Settings->bSubstepping)Step=FMath::Min(Step,Settings->MaxSubstepDeltaTime);
    PhysicsStep=FMath::Lerp(PhysicsStep,FMath::Clamp(Step,1.f/240.f,1.f/30.f),.04f);
    FVector At=Body->GetComponentLocation();double Now=GetWorld()->GetTimeSeconds();bool Held=Body->ComponentHasTag(TEXT("SSOFurnitureHeld"));
    if(Body->IsSimulatingPhysics() && !Held)
    {
        Body->SetLinearDamping(.003f);Body->SetAngularDamping(.32f);Body->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
        Body->SetPhysMaterialOverride(Rubber);
    }
    if(!Held && Now-ShotAt<8 && Now-ScoredAt>1.f && At.Z<Previous.Z)
        for(int I=0;I<2;++I)
        {
            FVector Rim=HouseCourt::Rim(I);
            if(FParse::Param(FCommandLine::Get(),TEXT("HouseBasketballReview")) && FVector::Dist2D(At,Rim)<240 && At.Z>HouseCourt::Floor+230)
                UE_LOG(LogTemp,Display,TEXT("COURT_APPROACH end=%d at=%s v=%s dt=%.4f"),I,*At.ToString(),*Body->GetPhysicsLinearVelocity().ToString(),GetWorld()->GetDeltaSeconds());
            if(Previous.Z>=Rim.Z && At.Z<Rim.Z)
            {
                FVector Crossing=FMath::Lerp(Previous,At,(Previous.Z-Rim.Z)/(Previous.Z-At.Z));
                if(FParse::Param(FCommandLine::Get(),TEXT("HouseBasketballReview")))UE_LOG(LogTemp,Display,TEXT("COURT_CROSS end=%d at=%s offset=%.1f v=%s"),I,*Crossing.ToString(),FVector::Dist2D(Crossing,Rim),*Body->GetPhysicsLinearVelocity().ToString());
                if(FVector::Dist2D(Crossing,Rim)<12.f)
                {
                    (I==0?LeftBaskets:RightBaskets)++;LastBasketAt=Now;ScoredAt=Now;ShotAt=-100;
                    ForceNetUpdate();UE_LOG(LogTemp,Display,TEXT("COURT_BASKET end=%d left=%d right=%d"),I,LeftBaskets,RightBaskets);
                }
            }
        }
    bool Outside=FMath::Abs(At.X)>1550 || At.Y<1700 || At.Y>3900;
    if(Held || !Outside)OutsideSince=-1;
    else if(OutsideSince<0)OutsideSince=Now;
    bool Lost=At.Z<-1200 || FMath::Abs(At.X)>19800 || FMath::Abs(At.Y)>19800;
    bool Abandoned=OutsideSince>0 && Now-OutsideSince>25 && Body->GetPhysicsLinearVelocity().Size()<20;
    if(!Held && (Lost || Abandoned))
    {
        Body->SetWorldLocation(HouseCourt::BallHome(),false,nullptr,ETeleportType::TeleportPhysics);Body->SetPhysicsLinearVelocity(FVector::ZeroVector);Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);Body->WakeAllRigidBodies();At=Body->GetComponentLocation();OutsideSince=-1;ShotAt=-100;ForceNetUpdate();
        UE_LOG(LogTemp,Display,TEXT("COURT_BALL_RECOVERED"));
    }
    Previous=At;
}
