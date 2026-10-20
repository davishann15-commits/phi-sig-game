#include "HouseCalderWildlifePrivate.h"
#include "HouseCalderDeerData.inl"
#include "HouseCalderDeerAnimation.inl"
#include "HouseCalderTerrain.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace HouseCalderWildlifePoseImpl
{
FTransform About(const FVector& Pivot,const FQuat& Rotation)
{return FTransform(Rotation,Pivot-Rotation.RotateVector(Pivot));}
FVector Normal(const FVector& P)
{
    const double X=HouseCalderTerrain::SurfaceZ(P.X+15,P.Y)-HouseCalderTerrain::SurfaceZ(P.X-15,P.Y);
    const double Y=HouseCalderTerrain::SurfaceZ(P.X,P.Y+15)-HouseCalderTerrain::SurfaceZ(P.X,P.Y-15);
    return FVector(-X,-Y,30).GetSafeNormal();
}
int32 Clip(ECalderWildlifeMood Mood)
{return Mood==ECalderWildlifeMood::Fleeing?1:HouseCalderWildlifePose::Speed(Mood)>0.?0:Mood==ECalderWildlifeMood::Feeding?2:3;}
double InverseTravel(double Distance,double Span,double Speed)
{
    if(Speed<=0.||Span<=.001)return 0.;const double Ramp=FMath::Min(Speed>500.?.24:.6,Span/Speed*.35),Duration=Span/Speed+Ramp;
    if(Distance<0.)return Distance/Speed;if(Distance>Span)return Duration+(Distance-Span)/Speed;
    if(Distance<Speed*Ramp*.5)return FMath::Sqrt(2.*Ramp*Distance/Speed);
    if(Distance>Span-Speed*Ramp*.5)return Duration-FMath::Sqrt(2.*Ramp*(Span-Distance)/Speed);
    return Distance/Speed+Ramp*.5;
}
}
double HouseCalderWildlifePose::Speed(ECalderWildlifeMood Mood)
{return Mood==ECalderWildlifeMood::Fleeing?900.:Mood==ECalderWildlifeMood::Returning?85.:Mood==ECalderWildlifeMood::Walking?65.:0.;}
bool HouseCalderWildlifePose::SameInputs(const FHouseCalderWildlifeState& A,const FHouseCalderWildlifeState& B)
{
    return A.Id==B.Id&&A.Species==B.Species&&A.Habitat==B.Habitat&&A.Variant==B.Variant&&A.Cause==B.Cause&&A.Mood==B.Mood&&A.Seed==B.Seed&&A.FacingYaw==B.FacingYaw&&A.StartTime==B.StartTime&&A.PreviousMood==B.PreviousMood&&A.PreviousPhase==B.PreviousPhase&&A.From==B.From&&A.To==B.To;
}
bool HouseCalderWildlifePose::CacheReview()
{
#if !UE_BUILD_SHIPPING
    static const bool Enabled=FParse::Param(FCommandLine::Get(),TEXT("CalderDeerPoseCacheReview"));return Enabled;
#else
    return false;
#endif
}
void HouseCalderWildlifePose::CheckCache(const FHouseCalderWildlifeAnimal& A,const FTransform& Cached,const FTransform& Fresh)
{
    ++A.CacheChecks;const double Position=FVector::Distance(Cached.GetTranslation(),Fresh.GetTranslation());
    // Packed rendering normalizes both quaternions. Chord distance avoids
    // acos(dot(q,q)) inventing an angle for identical, slightly nonunit inputs.
    const FQuat C=Cached.GetRotation().GetNormalized(),F=Fresh.GetRotation().GetNormalized();
    const double Chord=FMath::Sqrt(FMath::Min((C-F).SizeSquared(),(C+F).SizeSquared()));
    const double Angle=FMath::RadiansToDegrees(4.*FMath::Asin(FMath::Clamp(Chord*.5,0.,1.))),Scale=FVector::Distance(Cached.GetScale3D(),Fresh.GetScale3D());
    A.CachePositionErrorCm=FMath::Max(A.CachePositionErrorCm,Position);A.CacheAngleErrorDegrees=FMath::Max(A.CacheAngleErrorDegrees,Angle);A.CacheScaleError=FMath::Max(A.CacheScaleError,Scale);
    if(Position>1.e-4||Angle>1.e-4||Scale>1.e-7)UE_LOG(LogTemp,Error,TEXT("CALDER_DEER_POSE_CACHE_FAILED positionCm=%.12f angleDegrees=%.12f scale=%.12f"),Position,Angle,Scale);
}
double HouseCalderWildlifePose::AnimationPhase(const FHouseCalderWildlifeState& S,const FHouseCalderWildlifeAnimal& A,double Now)
{
    const double Movement=Speed(S.Mood),Offset=double(S.Seed%100u)/100.;
    if(Movement<=0.)return Now/HouseCalderDeerAnimation::Duration(HouseCalderWildlifePoseImpl::Clip(S.Mood))+Offset;
    const double Distance=HouseCalderWildlifePaths::TravelDistance(FMath::Max(0.,Now-S.StartTime),HouseCalderWildlifePaths::Length(A.Route),Movement);
    return Distance/(HouseCalderDeerAnimation::Stride(HouseCalderWildlifePoseImpl::Clip(S.Mood))*A.Scale)+Offset;
}
HouseCalderWildlifePose::FFootPhase HouseCalderWildlifePose::FootPhase(const FHouseCalderWildlifeState& S,const FHouseCalderWildlifeAnimal& A,int32 Leg,double Now)
{
    FFootPhase Out;Out.Moving=Speed(S.Mood)>0.;if(!Out.Moving){Out.Cycle=int64(S.StartTime*10.);Out.Touch=Out.Lift=Out.NextTouch=Now;return Out;}
    const int32 Clip=HouseCalderWildlifePoseImpl::Clip(S.Mood);const double TouchPhase=HouseCalderDeerAnimation::Touch(Clip,Leg),Stance=HouseCalderDeerAnimation::Stance(Clip,Leg);
    const double Phase=AnimationPhase(S,A,Now)-TouchPhase,Stride=HouseCalderDeerAnimation::Stride(Clip)*A.Scale,Offset=double(S.Seed%100u)/100.;
    Out.Cycle=FMath::FloorToInt64(Phase);Out.Local=FMath::Frac(Phase);if(Out.Local<0.)Out.Local+=1.;Out.Planted=Out.Local<Stance;
    const double StartDistance=(double(Out.Cycle)+TouchPhase-Offset)*Stride,Span=HouseCalderWildlifePaths::Length(A.Route),Movement=Speed(S.Mood);
    Out.Touch=S.StartTime+HouseCalderWildlifePoseImpl::InverseTravel(StartDistance,Span,Movement);
    Out.Lift=S.StartTime+HouseCalderWildlifePoseImpl::InverseTravel(StartDistance+Stance*Stride,Span,Movement);
    Out.NextTouch=S.StartTime+HouseCalderWildlifePoseImpl::InverseTravel(StartDistance+Stride,Span,Movement);return Out;
}
FVector HouseCalderWildlifePose::Root(const FHouseCalderWildlifeState& S,const FHouseCalderWildlifeAnimal& A,double Now,FVector* Direction)
{
    if(Direction)*Direction=FRotator(0,S.FacingYaw*360./65535.,0).Vector();if(Speed(S.Mood)<=0.)return S.From;
    const double Distance=HouseCalderWildlifePaths::TravelDistance(FMath::Max(0.,Now-S.StartTime),HouseCalderWildlifePaths::Length(A.Route),Speed(S.Mood));
    const FVector At=HouseCalderWildlifePaths::Evaluate(A.Route,Distance,Direction);
    if(Direction)
    {
        const FVector Before=HouseCalderWildlifePaths::Evaluate(A.Route,FMath::Max(0.,Distance-30.)),After=HouseCalderWildlifePaths::Evaluate(A.Route,Distance+30.);
        if(!After.Equals(Before,.01))*Direction=(After-Before).GetSafeNormal2D();
        const double Blend=FMath::SmoothStep(0.,Speed(S.Mood)>500.?.20:.65,Now-S.StartTime),Start=S.FacingYaw*360./65535.;
        *Direction=FRotator(0,Start+FRotator::NormalizeAxis(Direction->Rotation().Yaw-Start)*Blend,0).Vector();
    }
    return At;
}
FTransform HouseCalderWildlifePose::Frame(const FHouseCalderWildlifeState& S,const FHouseCalderWildlifeAnimal& A,double Now)
{
    FVector Direction;const FVector At=Root(S,A,Now,&Direction);
    const FVector Forward=Direction.GetSafeNormal2D(),Right=FVector::CrossProduct(FVector::UpVector,Forward);
    const double Pitch=(HouseCalderTerrain::SurfaceZ(At.X+Forward.X*80.,At.Y+Forward.Y*80.)-HouseCalderTerrain::SurfaceZ(At.X-Forward.X*80.,At.Y-Forward.Y*80.))/160.;
    const double Roll=(HouseCalderTerrain::SurfaceZ(At.X+Right.X*30.,At.Y+Right.Y*30.)-HouseCalderTerrain::SurfaceZ(At.X-Right.X*30.,At.Y-Right.Y*30.))/60.;
    const FVector Up=(FVector::UpVector-Forward*FMath::Clamp(Pitch,-.65,.65)-Right*FMath::Clamp(Roll,-.65,.65)).GetSafeNormal();
    const FVector Tangent=(Forward-Up*FVector::DotProduct(Forward,Up)).GetSafeNormal();
    return FTransform(FRotationMatrix::MakeFromXZ(Tangent,Up).ToQuat(),At,FVector(A.Scale));
}
FTransform HouseCalderWildlifePose::StanceSole(const FHouseCalderWildlifeState& S,const FHouseCalderWildlifeAnimal& A,int32 Leg,double TouchTime)
{
    auto Compute=[&]
    {
        const FTransform BodyFrame=Frame(S,A,TouchTime);const double Scale=A.Scale;const int32 Clip=HouseCalderWildlifePoseImpl::Clip(S.Mood),Joint=7+Leg*4;
        const FVector Hoof=HouseCalderDeerData::Hoof(Leg);const bool Moving=Speed(S.Mood)>0.;
        const FTransform Local=HouseCalderDeerAnimation::Sample(Moving?Clip:4,Moving?HouseCalderDeerAnimation::Touch(Clip,Leg):0.,Joint);
        FVector Foot=BodyFrame.TransformPosition(Local.TransformPosition(Hoof));Foot.Z=HouseCalderTerrain::SurfaceZ(Foot.X,Foot.Y);
        const FQuat Rotation=FRotationMatrix::MakeFromXZ(BodyFrame.GetRotation().GetForwardVector(),HouseCalderWildlifePoseImpl::Normal(Foot)).ToQuat();
        return FTransform(Rotation,Foot-Rotation.RotateVector(Hoof*Scale),FVector(Scale));
    };
    // Authored terrain is immutable. Within one step, this transform depends
    // only on the state, scale and touchdown time; standing has no time input.
    if(!A.StanceStateValid||A.StanceScale!=A.Scale||!SameInputs(S,A.StanceState))
    {for(auto& Limb:A.StanceMemo)for(auto& Entry:Limb)Entry.Valid=false;A.StanceState=S;A.StanceScale=A.Scale;A.StanceStateValid=true;}
    const double Key=Speed(S.Mood)>0.?TouchTime:0.;
    for(const auto& Entry:A.StanceMemo[Leg])if(Entry.Valid&&Entry.Time==Key)
    {++A.MemoHits;if(CacheReview())CheckCache(A,Entry.Pose,Compute());return Entry.Pose;}
    ++A.MemoMisses;auto& Entry=A.StanceMemo[Leg][A.StanceCursor[Leg]];A.StanceCursor[Leg]=(A.StanceCursor[Leg]+1)%3;Entry.Time=Key;Entry.Pose=Compute();Entry.Valid=true;return Entry.Pose;
}
void HouseCalderWildlifePose::Evaluate(const FHouseCalderWildlifeState& S,FHouseCalderWildlifeAnimal& A,double Now,double SceneNow,
    FTransform& RootPose,FTransform& Tail,FTransform& Antlers,FTransform& Contact,TArray<FTransform>& Out)
{
    using namespace HouseCalderWildlifePoseImpl;FVector Direction;FVector At=Root(S,A,Now,&Direction);At.Z=HouseCalderTerrain::SurfaceZ(At.X,At.Y);
    const bool Flee=S.Mood==ECalderWildlifeMood::Fleeing,Moving=Speed(S.Mood)>0.,Feeding=S.Mood==ECalderWildlifeMood::Feeding;
    const double Age=FMath::Max(0.,Now-S.StartTime),Scale=A.Scale,Phase=Moving?AnimationPhase(S,A,Now):SceneNow/HouseCalderDeerAnimation::Duration(Clip(S.Mood))+double(S.Seed%100u)/100.;
    const double Period=7.+double(S.Seed%6u),Idle=FMath::Fmod(SceneNow+double(S.Seed%97u),Period);
    const double Watch=FMath::SmoothStep(Period-3.,Period-2.,Idle)*(1.-FMath::SmoothStep(Period-.6,Period,Idle));
    const double Stamp=S.Mood==ECalderWildlifeMood::Alarm?FMath::Max(0.,FMath::Sin(FMath::Fmod(Age,3.2)*PI/.65)):0.;
    RootPose=Frame(S,A,Now);Out.SetNum(Controls);
    FTransform Local[Controls];const double Transition=FMath::SmoothStep(0.,.45,Age);
    for(int32 Joint=0;Joint<Controls;++Joint)
    {
        Local[Joint]=HouseCalderDeerAnimation::Sample(Clip(S.Mood),Phase,Joint);
        if(Feeding){FTransform Raised=HouseCalderDeerAnimation::Sample(3,SceneNow/HouseCalderDeerAnimation::Duration(3)+S.Seed*.001,Joint);Local[Joint].Blend(Local[Joint],Raised,float(Watch));}
        if(Transition<1. && S.Cause!=3)
        {const FTransform Previous=HouseCalderDeerAnimation::Sample(Clip(S.PreviousMood),S.PreviousPhase,Joint);Local[Joint].Blend(Previous,Local[Joint],float(Transition));}
        Out[Joint]=Local[Joint]*RootPose;
    }
    if(S.Mood==ECalderWildlifeMood::Staring||S.Mood==ECalderWildlifeMood::Alarm)
    {
        const double Yaw=FMath::Clamp(FRotator::NormalizeAxis((FVector(S.To)-At).Rotation().Yaw-RootPose.Rotator().Yaw),-55.f,55.f)*FMath::SmoothStep(0.,.4,Age);
        const FTransform Turn=About(HouseCalderDeerData::Pivot(1),FRotator(0,Yaw,0).Quaternion());
        for(int32 Joint:{1,2,3,20})Out[Joint]=(Local[Joint]*Turn)*RootPose;
    }
    for(int32 Leg=0;Leg<4;++Leg)
    {
        const int32 FirstJoint=4+Leg*4,SoleJoint=FirstJoint+3;const FVector Hoof=HouseCalderDeerData::Hoof(Leg);
        const FFootPhase Foot=FootPhase(S,A,Leg,Now);const FVector RawFoot=Out[SoleJoint].TransformPosition(Hoof);
        const FTransform Anchor=StanceSole(S,A,Leg,Foot.Touch);FVector Wanted=Anchor.TransformPosition(Hoof);bool Planted=Foot.Planted;
        if(Moving&&!Planted)
        {
            // Keep the actual donor hand/hock/toe motion in the swing and
            // aerial phases. The former two-bone solve replaced that motion
            // with a straight synthetic leg and destroyed the run silhouette.
            Wanted=RawFoot;const double Lift=FMath::Max(0.,Local[SoleJoint].TransformPosition(Hoof).Z-HouseCalderDeerAnimation::Floor(Clip(S.Mood),Leg));
            Wanted.Z=HouseCalderTerrain::SurfaceZ(Wanted.X,Wanted.Y)+Lift*Scale;
        }
        else if(!Moving&&Speed(S.PreviousMood)>0.&&Age<.6)
        {
            FVector Before=RootPose.TransformPosition(HouseCalderDeerAnimation::Sample(Clip(S.PreviousMood),S.PreviousPhase,SoleJoint).TransformPosition(Hoof));Before.Z=HouseCalderTerrain::SurfaceZ(Before.X,Before.Y);
            const double Step=FMath::SmoothStep(Leg*.07,Leg*.07+.32,Age);Wanted=FMath::Lerp(Before,Wanted,Step);Wanted.Z+=FMath::Sin(PI*Step)*3.*Scale;Planted=Step>=1.;
        }
        const double StampLift=S.Mood==ECalderWildlifeMood::Alarm&&Leg==0?Stamp*11.*Scale:0.;Wanted.Z+=StampLift;
        const FVector Correction=Wanted-RawFoot;
        // Preserve all four donor joints. A small distributed translation
        // supplies slope/stance correction without inventing new knee angles.
        const double Weights[]={0.,.22,.67,1.};
        for(int32 K=0;K<4;++K)Out[FirstJoint+K].AddToTranslation(Correction*Weights[K]);
        if(Planted)
        {
            const FQuat SoleRotation=Anchor.GetRotation();Out[SoleJoint]=FTransform(SoleRotation,Wanted-SoleRotation.RotateVector(Hoof*Scale),FVector(Scale));
        }
        A.Hooves[Leg]=Wanted;A.Planted[Leg]=Planted&&StampLift<.1;A.CurrentFootCycles[Leg]=Foot.Cycle;
    }
    Tail=FTransform(FRotator(float(Flee?-80.:48.),float(FMath::Sin(SceneNow*1.6+S.Seed)*2.),0),HouseCalderDeerData::TailRoot())*Out[0];
    Antlers=FTransform(FRotator::ZeroRotator,FVector(73,0,139))*Out[20];
    Contact=FTransform(FRotationMatrix::MakeFromXZ(Direction,Normal(At)).ToQuat(),At+FVector(0,0,1.2),FVector(Scale*1.1,Scale*.72,1.));
}
