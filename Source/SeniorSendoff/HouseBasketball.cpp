#include "HouseBasketball.h"
#include "HouseBasketballCourt.h"
#include "HouseBasketballHoop.h"
#include "HouseSfx.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallPaint.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
    // Replace the generic furniture knock with basketball-specific contact sounds.
    Body->OnComponentHit.RemoveAll(this);
    Body->OnComponentHit.AddDynamic(this, &AHouseBasketball::OnBallHit);
    Rubber=NewObject<UPhysicalMaterial>(this);Rubber->Friction=.45f;Rubber->StaticFriction=.5f;
    Rubber->Restitution=.76f;Rubber->RestitutionCombineMode=EFrictionCombineMode::Max;Rubber->bOverrideRestitutionCombineMode=true;
    Body->SetPhysMaterialOverride(Rubber);Body->SetUseCCD(true);
    SfxSurface=EHouseSurface::Plastic;Previous=Body->GetComponentLocation();
    RecoveryHome=Previous;
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
FVector AHouseBasketball::ReleasePoint(const FVector& View, const FRotator& Aim)
{
    return View + FVector(Aim.Vector().X, Aim.Vector().Y, 0).GetSafeNormal() * 55 + FVector(0, 0, 12);
}
float AHouseBasketball::SuggestedPower(const FVector& View, const FRotator& Aim) const
{
    const FVector From = Body->GetComponentLocation();
    if (FMath::Abs(From.X) > 1550 || From.Y < 1700 || From.Y > 3900) return -1;
    float Best = .85f, Power = -1;
    for (int I = 0; I < 2; ++I)
    {
        const FVector Rim = HouseCourt::Rim(I), LaunchFrom = ReleasePoint(View, Aim);
        const float Dot = FVector::DotProduct((Rim - View).GetSafeNormal(), Aim.Vector());
        const float Distance = FVector::Dist2D(LaunchFrom, Rim);
        if (Dot < Best || Distance < 90 || Distance > 2800) continue;
        // The reference band assumes the crosshair is ON the rim. Raising/lowering
        // the actual aim changes the release angle and can still miss a green shot.
        const float Pitch = (Rim - View).Rotation().Pitch;
        const float Angle = FMath::DegreesToRadians(FMath::Clamp(50.f + Pitch * .45f, 25.f, 78.f));
        const float Denom = 2 * FMath::Square(FMath::Cos(Angle)) * (Distance * FMath::Tan(Angle) - (Rim.Z - LaunchFrom.Z));
        if (Denom <= 0) continue;
        const float Speed = FMath::Sqrt(-GetWorld()->GetGravityZ() * Distance * Distance / Denom);
        if (Speed < 500 || Speed > 1850) continue;
        Best = Dot; Power = (Speed - 500) / 1350;
    }
    return Power;
}
FVector AHouseBasketball::ShotVelocity(const FVector& View, const FRotator& Aim, float Power) const
{
    const float Ideal = SuggestedPower(View, Aim);
    // A timing window makes the meter playable at normal frame rates. Only the
    // magnitude is calibrated; yaw and pitch are always the player's own aim.
    if (Ideal >= 0 && FMath::Abs(Power - Ideal) <= GreenHalfWidth) Power = Ideal;
    const float Angle = FMath::DegreesToRadians(FMath::Clamp(50.f + FRotator::NormalizeAxis(Aim.Pitch) * .45f, 25.f, 78.f));
    const FVector Flat = FVector(Aim.Vector().X, Aim.Vector().Y, 0).GetSafeNormal();
    FVector Velocity = (Flat * FMath::Cos(Angle) + FVector::UpVector * FMath::Sin(Angle)) * SpeedForPower(Power);
    // Small integration/air-drag compensation, not a hoop-seeking correction.
    Velocity *= 1.002f; Velocity.Z += -GetWorld()->GetGravityZ() * PhysicsStep * .5f;
    return Velocity;
}
void AHouseBasketball::OnBallHit(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent* Surface, FVector Impulse, const FHitResult& Hit)
{
    if (!HasAuthority() || !Surface || Body->ComponentHasTag(TEXT("SSOFurnitureHeld"))) return;
    const float Speed = FMath::Max(float(Impulse.Size()) / .62f,
        float(FMath::Abs(FVector::DotProduct(Body->GetPhysicsLinearVelocity(), Hit.ImpactNormal))));
    if (auto* Hoop = Cast<AHouseBasketballHoop>(Other))
    { Hoop->BallImpact(Surface, Hit.ImpactPoint, -Hit.ImpactNormal * Speed, Speed); return; }
    if (Speed > 85 && GetWorld()->GetTimeSeconds() - LastBounceAt > .12)
    { LastBounceAt = GetWorld()->GetTimeSeconds(); MulticastBounce(Hit.ImpactPoint, uint8(FMath::Clamp(Speed / 5, 1.f, 255.f))); }
}
void AHouseBasketball::MulticastBounce_Implementation(FVector_NetQuantize Point, uint8 Strength)
{ HouseSfx::Basketball(GetWorld(), Point, 0, Strength * 5.f); }
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
            if((FParse::Param(FCommandLine::Get(),TEXT("HouseBasketballReview")) || FParse::Param(FCommandLine::Get(),TEXT("HouseBasketballSkillReview"))) && FVector::Dist2D(At,Rim)<240 && At.Z>HouseCourt::Floor+230)
                UE_LOG(LogTemp,Display,TEXT("COURT_APPROACH end=%d at=%s v=%s dt=%.4f"),I,*At.ToString(),*Body->GetPhysicsLinearVelocity().ToString(),GetWorld()->GetDeltaSeconds());
            if(Previous.Z>=Rim.Z && At.Z<Rim.Z)
            {
                FVector Crossing=FMath::Lerp(Previous,At,(Previous.Z-Rim.Z)/(Previous.Z-At.Z));
                if(FParse::Param(FCommandLine::Get(),TEXT("HouseBasketballReview")) || FParse::Param(FCommandLine::Get(),TEXT("HouseBasketballSkillReview")))UE_LOG(LogTemp,Display,TEXT("COURT_CROSS end=%d at=%s offset=%.1f v=%s"),I,*Crossing.ToString(),FVector::Dist2D(Crossing,Rim),*Body->GetPhysicsLinearVelocity().ToString());
                if(FVector::Dist2D(Crossing,Rim)<12.f)
                {
                    (I==0?LeftBaskets:RightBaskets)++;LastBasketAt=Now;ScoredAt=Now;ShotAt=-100;
                    ForceNetUpdate();UE_LOG(LogTemp,Display,TEXT("COURT_BASKET end=%d left=%d right=%d"),I,LeftBaskets,RightBaskets);
                    for (TActorIterator<AHouseBasketballHoop> It(GetWorld()); It; ++It)
                        if (It->End() == I) It->Basket(Body->GetPhysicsLinearVelocity());
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
        Body->SetWorldLocation(RecoveryHome,false,nullptr,ETeleportType::TeleportPhysics);Body->SetPhysicsLinearVelocity(FVector::ZeroVector);Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);Body->WakeAllRigidBodies();At=Body->GetComponentLocation();OutsideSince=-1;ShotAt=-100;ForceNetUpdate();
        UE_LOG(LogTemp,Display,TEXT("COURT_BALL_RECOVERED"));
    }
    Previous=At;
}
