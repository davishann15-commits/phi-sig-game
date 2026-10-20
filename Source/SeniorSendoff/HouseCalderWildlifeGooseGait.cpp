// Two alternating world-space foot plants. Medium-rate swings clamp their
// interpolation keys at lift-off/touchdown, as the verified deer gait does.
#include "HouseCalderWildlifeGooseGait.h"
#include "HouseCalderWildlifeBirds.h"
#include "HouseCalderTerrain.h"
namespace HouseCalderWildlifeGooseGaitImpl
{
using namespace HouseCalderWildlifeGooseGait;
FFoot Anchor(HouseCalderWildlifeGooseGait::FSampler Evaluate,
    double Raw,double Age,double Touch,double Frequency,int32 Leg)
{
    HouseCalderWildlifeGoosePaths::FCycleSample At;
    // First step begins at the idle foot, subsequent touchdowns lead the body
    // by half a stance. The validated route clamps naturally at either end.
    const double Offset=Touch<=0.?-Age:Touch+.32/Frequency-Age;
    Evaluate(Raw+Offset,At);
    const FTransform Root(At.Bird.Direction.Rotation(),At.Bird.Position,FVector(At.Bird.Scale));
    FFoot Foot;Foot.Sole=Root.TransformPosition(FVector(-17.,Leg==0?5.:-5.,0.));
    const FVector P=Foot.Sole;Foot.Sole.Z=HouseCalderTerrain::SurfaceZ(P.X,P.Y);
    const double DX=(HouseCalderTerrain::SurfaceZ(P.X+10.,P.Y)-HouseCalderTerrain::SurfaceZ(P.X-10.,P.Y))/20.,DY=(HouseCalderTerrain::SurfaceZ(P.X,P.Y+10.)-HouseCalderTerrain::SurfaceZ(P.X,P.Y-10.))/20.;
    Foot.Normal=FVector(-DX,-DY,1.).GetSafeNormal();Foot.Forward=At.Bird.Direction;return Foot;
}
FFoot Swing(const FFoot& A,const FFoot& B,double Local,double Scale)
{
    const double U=FMath::Clamp((Local-.64)/.36,0.,1.),Blend=FMath::SmoothStep(0.,1.,U);
    FFoot F;F.Sole=FMath::Lerp(A.Sole,B.Sole,Blend)+FVector(0,0,4.*Scale*FMath::Sin(PI*U));
    F.Normal=FMath::Lerp(A.Normal,B.Normal,Blend).GetSafeNormal();
    const FQuat Q=FQuat::Slerp(A.Forward.Rotation().Quaternion(),B.Forward.Rotation().Quaternion(),Blend);F.Forward=Q.GetForwardVector();return F;
}
}
void HouseCalderWildlifeGooseGait::Apply(FSampler Evaluate,
    double Raw,const HouseCalderWildlifeGoosePaths::FCycleSample& Sample,
    const FTransform& Root,bool Medium,FCache& Cache,TArray<FTransform>& Controls)
{
    using namespace HouseCalderWildlifeGooseGaitImpl;
    if(Sample.ShoreRoute<0 || Sample.Stand<=.001){Cache.Start=-1.e30;return;}
    const bool Moving=Sample.WalkAge>=0.;const double Age=Moving?Sample.WalkAge:0.,Frequency=1.2/Sample.Bird.Scale,Start=Raw-Age;
    if(Cache.Route!=Sample.ShoreRoute || (Moving&&FMath::Abs(Cache.Start-Start)>.001) || (!Moving&&Cache.Start!=-1.))
    {Cache.Route=Sample.ShoreRoute;Cache.Start=Moving?Start:-1.;Cache.Cycles[0]=Cache.Cycles[1]=MIN_int64;}
    FVector Soles[2],Normals[2],Forwards[2];
    for(int32 Leg=0;Leg<2;++Leg)
    {
        const double Offset=Leg*.5,Phase=Age*Frequency+Offset,Local=FMath::Frac(Phase);
        const int64 Cycle=Moving?FMath::FloorToInt64(Phase):0;const double Touch=Moving?(double(Cycle)-Offset)/Frequency:0.;
        if(Cache.Cycles[Leg]!=Cycle)
        {
            Cache.Stance[Leg]=Moving&&Cache.Cycles[Leg]!=MIN_int64&&Cache.Cycles[Leg]+1==Cycle?Cache.Next[Leg]:Anchor(Evaluate,Raw,Age,Touch,Frequency,Leg);
            Cache.Next[Leg]=Moving?Anchor(Evaluate,Raw,Age,Touch+1./Frequency,Frequency,Leg):Cache.Stance[Leg];Cache.Cycles[Leg]=Cycle;
        }
        Cache.Planted[Leg]=!Moving||Local<.64;Cache.CurrentCycles[Leg]=Cycle;FFoot Foot=Cache.Stance[Leg];
        if(!Cache.Planted[Leg])
        {
            if(!Medium)Foot=Swing(Cache.Stance[Leg],Cache.Next[Leg],Local,Sample.Bird.Scale);
            else
            {
                const double Lift=Touch+.64/Frequency,Land=Touch+1./Frequency,Key=FMath::FloorToDouble(Raw*15.)/15.;
                const double From=FMath::Max(Key,Start+Lift),To=FMath::Min(Key+1./15.,Start+Land);
                const FFoot A=Swing(Cache.Stance[Leg],Cache.Next[Leg],((From-Start)-Touch)*Frequency,Sample.Bird.Scale),B=Swing(Cache.Stance[Leg],Cache.Next[Leg],((To-Start)-Touch)*Frequency,Sample.Bird.Scale);
                const double Alpha=FMath::Clamp((Raw-From)/FMath::Max(1.e-9,To-From),0.,1.);Foot.Sole=FMath::Lerp(A.Sole,B.Sole,Alpha);Foot.Normal=FMath::Lerp(A.Normal,B.Normal,Alpha).GetSafeNormal();Foot.Forward=FQuat::Slerp(A.Forward.Rotation().Quaternion(),B.Forward.Rotation().Quaternion(),Alpha).GetForwardVector();
            }
        }
        Soles[Leg]=Foot.Sole;Normals[Leg]=Foot.Normal;Forwards[Leg]=Foot.Forward;
    }
    TArray<FTransform> Ground=Controls;HouseCalderWildlifeBirds::GroundLegs(Root,Soles,Normals,Ground,Forwards);
    for(int32 Joint:{10,11,12,13})Controls[Joint].Blend(Controls[Joint],Ground[Joint],Sample.Stand);
}
