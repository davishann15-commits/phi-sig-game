#pragma once
#include "CoreMinimal.h"
#include "ReferenceSkeleton.h"

// Lobby-only choreography. Gameplay projectiles never use this timeline.
struct FSeniorDouliIdle
{
    FVector Wrist;
    FQuat Rotation;
    FTransform FreeHat=FTransform::Identity;
    enum class EAttachment : uint8 { Hand, Air, Head, LeftHand };
    EAttachment Attachment=EAttachment::Hand;
    float Grip=1.f;
    float HeadFollow=0.f;
    FVector LeftWrist=FVector(33,6,87);
    FQuat LeftRotation=FQuat::Identity;
    float LeftGrip=0.f,LeftBlend=0.f;
    static constexpr float Duration=40.f;
    static constexpr float Gravity=980.f;
    static constexpr float PassFlight=.44f;
    static FVector Ballistic(const FVector& From,const FVector& To,float Seconds,float Flight)
    {
        const FVector Velocity=(To-From)/Flight+FVector(0,0,.5f*Gravity*Flight);
        return From+Velocity*Seconds-FVector(0,0,.5f*Gravity*Seconds*Seconds);
    }
    static float Ease(float U) { U=FMath::Clamp(U,0.f,1.f); return U*U*U*(U*(U*6.f-15.f)+10.f); }
    static FTransform RefBone(const FReferenceSkeleton& Ref,const TCHAR* Name)
    {
        int32 I=Ref.FindBoneIndex(Name); FTransform T=FTransform::Identity;
        while(I!=INDEX_NONE) { T=T*Ref.GetRefBonePose()[I]; I=Ref.GetParentIndex(I); }
        return T;
    }
    static FVector WornLocation() { return FVector(0,1,171.5f); }
    static FTransform HeadHat(const FReferenceSkeleton& Ref)
    {
        return FTransform(FQuat::Identity,WornLocation()).GetRelativeTransform(RefBone(Ref,TEXT("head")));
    }
    // Hat +X points from the gripping edge towards the crown. Place the rim
    // at the fingers, not at the wrist. This frame is constant in hand space.
    static FVector HatOffset(bool Right=true) { return FVector(25.8,Right?11:-11,-2); }
    static FQuat PalmFrame(bool Right=true)
    {
        // Palm faces the edge, thumb above it, fingers running tangentially
        // underneath. A palm-down pose makes the disc sit on the knuckles.
        return FRotationMatrix::MakeFromXZ(FVector(0,Right?1:-1,-.15).GetSafeNormal(),FVector(-1,0,0)).ToQuat();
    }
    static FQuat HandFrame(const FReferenceSkeleton& Ref,bool Right=true)
    {
        auto Bone=[&](const TCHAR* Name) {
            int32 I=Ref.FindBoneIndex(Name);
            FTransform T=FTransform::Identity;
            while(I!=INDEX_NONE) { T=T*Ref.GetRefBonePose()[I]; I=Ref.GetParentIndex(I); }
            return T;
        };
        const FTransform H=Bone(Right?TEXT("hand_r"):TEXT("hand_l"));
        const FVector F=H.InverseTransformPosition(Bone(Right?TEXT("middle_01_r"):TEXT("middle_01_l")).GetLocation()).GetSafeNormal();
        const FVector Across=H.InverseTransformVectorNoScale(Bone(Right?TEXT("index_01_r"):TEXT("index_01_l")).GetLocation()-Bone(Right?TEXT("pinky_01_r"):TEXT("pinky_01_l")).GetLocation()).GetSafeNormal();
        return FRotationMatrix::MakeFromXZ(F,FVector::CrossProduct(Across,F)*(Right?1.f:-1.f)).ToQuat();
    }
    static FSeniorDouliIdle At(float Seconds)
    {
        const float T=FMath::Fmod(FMath::Max(0.f,Seconds),Duration);
        const FQuat RestR=FRotator(-45,180,0).Quaternion();
        const FVector RestWrist(-33+.65f*FMath::Sin(2*PI*T/Duration),6+FMath::Sin(10*PI*T/Duration),87+.5f*FMath::Sin(20*PI*T/Duration));
        const FTransform Rest(RestR,RestWrist+RestR.RotateVector(HatOffset()));
        const FTransform Toss(FRotator(0,90,0).Quaternion(),FVector(-48,62,118));
        const FTransform Dip(Toss.GetRotation(),Toss.GetLocation()-FVector(0,0,18));
        // Clear the face before moving over the crown; reverse this route on removal.
        const FTransform Approach(FQuat::Identity,FVector(-22,42,178));
        const FTransform Above(FQuat::Identity,FVector(0,1,190));
        const FTransform Worn(FQuat::Identity,WornLocation());
        auto Blend=[](const FTransform& A,const FTransform& B,float U) {
            const float E=Ease(U);
            return FTransform(FQuat::Slerp(A.GetRotation(),B.GetRotation(),E),FMath::Lerp(A.GetLocation(),B.GetLocation(),E));
        };
        // Shared waypoint velocities keep the arm moving through intermediate
        // poses instead of stopping separately at every stage of the gesture.
        auto Journey=[](const FTransform* Keys,const float* Times,int32 Count,float Now) {
            int32 I=0; while(I<Count-2 && Now>Times[I+1])++I;
            const float Span=Times[I+1]-Times[I];
            const float U=FMath::Clamp((Now-Times[I])/Span,0.f,1.f);
            auto Velocity=[&](int32 K) {
                return K==0 || K==Count-1 ? FVector::ZeroVector :
                    (Keys[K+1].GetLocation()-Keys[K-1].GetLocation())/(Times[K+1]-Times[K-1]);
            };
            return FTransform(FQuat::Slerp(Keys[I].GetRotation(),Keys[I+1].GetRotation(),Ease(U)),
                FMath::CubicInterp(Keys[I].GetLocation(),Velocity(I)*Span,Keys[I+1].GetLocation(),Velocity(I+1)*Span,U));
        };
        FSeniorDouliIdle P;
        FTransform Held=Rest;
        if(T>=4 && T<5.5) Held=Blend(Rest,Toss,(T-4)/1.5f);
        else if(T>=5.5 && T<5.88) Held=Blend(Toss,Dip,(T-5.5f)/.38f);
        else if(T>=5.88 && T<6)
        {
            const float U=(T-5.88f)/.12f;
            // Launch tangent matches the upward flight, avoiding a stop at release.
            Held=FTransform(Toss.GetRotation(),FMath::CubicInterp(Dip.GetLocation(),FVector::ZeroVector,Toss.GetLocation(),FVector(0,0,.5f*Gravity*.64f*.12f),U));
        }
        else if(T>=6 && T<6.64)
        {
            const float U=(T-6)/.64f;
            P.Attachment=EAttachment::Air;
            // A small spinning toss, with a real release and a return to the same grip.
            P.FreeHat=FTransform(FRotator(0,90+360*U,0).Quaternion(),Ballistic(Toss.GetLocation(),Toss.GetLocation(),T-6,.64f));
            Held=FTransform(Toss.GetRotation(),Toss.GetLocation()+FVector(0,0,10*FMath::Sin(PI*U)));
            P.Grip=1-FMath::Min(Ease(U/.16f),1-Ease((U-.84f)/.16f));
        }
        else if(T>=6.64 && T<6.82)
            Held=FTransform(Toss.GetRotation(),FMath::CubicInterp(Toss.GetLocation(),FVector(0,0,-.5f*Gravity*.64f*.18f),Dip.GetLocation(),FVector::ZeroVector,(T-6.64f)/.18f));
        else if(T>=6.82 && T<10) Held=Blend(Dip,Rest,(T-6.82f)/3.18f);
        else if(T>=13 && T<14.4) Held=Blend(Rest,Toss,(T-13)/1.4f);
        else if(T>=14.4 && T<15.5) Held=Blend(Toss,Approach,(T-14.4f)/1.1f);
        else if(T>=15.5 && T<17) Held=Blend(Approach,Above,(T-15.5f)/1.5f);
        else if(T>=17 && T<18) { Held=Blend(Above,Worn,T-17); P.HeadFollow=Ease(T-17); }
        else if(T>=18 && T<18.5) { Held=Worn; P.HeadFollow=1; }
        else if(T>=18.5 && T<23.8)
        {
            P.Attachment=EAttachment::Head;
            P.FreeHat=Worn;
            if(T<19.8) { Held=Blend(Worn,Rest,(T-18.5f)/1.3f); P.HeadFollow=1-Ease((T-18.5f)/1.3f); P.Grip=1-Ease((T-18.5f)/.3f); }
            else if(T<22.5) { Held=Rest; P.Grip=0; }
            else { Held=Blend(Rest,Worn,(T-22.5f)/1.3f); P.HeadFollow=Ease((T-22.5f)/1.3f); P.Grip=Ease((T-23.4f)/.4f); }
        }
        else if(T>=23.8 && T<25) { Held=Blend(Worn,Above,(T-23.8f)/1.2f); P.HeadFollow=1-Ease((T-23.8f)/1.2f); }
        else if(T>=25 && T<26.8) Held=Blend(Above,Approach,(T-25)/1.8f);
        else if(T>=26.8 && T<27.7) Held=Blend(Approach,Toss,(T-26.8f)/.9f);
        else if(T>=27.7 && T<29) Held=Blend(Toss,Rest,(T-27.7f)/1.3f);
        if(T>=30 && T<38.8)
        {
            const FQuat PassR=FRotator(0,90,0).Quaternion();
            const FTransform RightPass(PassR,FVector(-44,60,112)),LeftPass(PassR,FVector(44,60,112));
            P.LeftRotation=PassR;
            P.LeftBlend=T<32?Ease((T-30)/2.f):(T>=36.3?1-Ease((T-36.3f)/2.5f):1.f);
            P.LeftWrist=LeftPass.GetLocation()-PassR.RotateVector(HatOffset(false));
            Held=T<31.7?Blend(Rest,RightPass,(T-30)/1.7f):(T>=36.3?Blend(RightPass,Rest,(T-36.3f)/2.5f):RightPass);
            const FVector PrepOffset(-8,0,-6);
            if(T>=31.7 && T<31.88) Held=Blend(RightPass,FTransform(PassR,RightPass.GetLocation()+PrepOffset),(T-31.7f)/.18f);
            if(T>=31.88 && T<32)
            {
                const FVector LaunchVelocity=(LeftPass.GetLocation()-RightPass.GetLocation())/PassFlight+FVector(0,0,.5f*Gravity*PassFlight);
                Held=FTransform(PassR,FMath::CubicInterp(RightPass.GetLocation()+PrepOffset,FVector::ZeroVector,RightPass.GetLocation(),LaunchVelocity*.12f,(T-31.88f)/.12f));
            }
            if(T>=32 && T<36.3)
            {
                const float LegStarts[]={32.f,33.1f,34.2f,35.3f};
                int32 Leg=0; while(Leg<3 && T>=LegStarts[Leg+1])++Leg;
                const bool ToLeft=Leg%2==0;
                const float Elapsed=T-LegStarts[Leg],U=Elapsed/PassFlight;
                const FVector From=ToLeft?RightPass.GetLocation():LeftPass.GetLocation();
                const FVector To=ToLeft?LeftPass.GetLocation():RightPass.GetLocation();
                P.Grip=0; P.LeftGrip=0;
                if(U<1)
                {
                    P.Attachment=EAttachment::Air;
                    // Once released: constant lateral speed, gravity, constant spin.
                    // No ease-in/out or lingering at the apex while airborne.
                    P.FreeHat=FTransform(FRotator(0,90+(ToLeft?360:-360)*U,0).Quaternion(),Ballistic(From,To,Elapsed,PassFlight));
                    const float Release=1-Ease(U/.18f),Catch=Ease((U-.8f)/.2f);
                    P.Grip=ToLeft?Release:Catch; P.LeftGrip=ToLeft?Catch:Release;
                    Held.AddToTranslation(FVector(ToLeft?6:-2,0,4)*FMath::Sin(PI*U));
                    P.LeftWrist+=FVector(ToLeft?2:-6,0,4)*FMath::Sin(PI*U);
                }
                else
                {
                    P.Attachment=ToLeft?EAttachment::LeftHand:EAttachment::Hand;
                    P.Grip=ToLeft?0:1; P.LeftGrip=ToLeft?1:0;
                    const float SinceCatch=Elapsed-PassFlight;
                    const FVector Direction=(To-From).GetSafeNormal();
                    const FVector ImpactVelocity=(To-From)/PassFlight-FVector(0,0,.5f*Gravity*PassFlight);
                    const FVector Absorbed=To+Direction*4-FVector(0,0,8);
                    const FVector Prepared=To+Direction*8-FVector(0,0,6);
                    const FVector CarryVelocity=Leg==3 ? FVector::ZeroVector : Direction*10.f;
                    FVector Contact;
                    if(SinceCatch<.14f)
                        Contact=FMath::CubicInterp(To,ImpactVelocity*.14f,Absorbed,CarryVelocity*.14f,SinceCatch/.14f);
                    else if(Leg==3)
                        Contact=FMath::Lerp(Absorbed,To,Ease((SinceCatch-.14f)/.42f));
                    else if(Elapsed<.98f)
                        Contact=FMath::CubicInterp(Absorbed,CarryVelocity*.4f,Prepared,FVector::ZeroVector,FMath::Clamp((SinceCatch-.14f)/.4f,0.f,1.f));
                    else
                    {
                        const FVector NextVelocity=(From-To)/PassFlight+FVector(0,0,.5f*Gravity*PassFlight);
                        Contact=FMath::CubicInterp(Prepared,FVector::ZeroVector,To,NextVelocity*.12f,(Elapsed-.98f)/.12f);
                    }
                    if(ToLeft)P.LeftWrist+=Contact-To; else Held.AddToTranslation(Contact-To);
                }
            }
        }
        if(T>=13 && T<18)
        {
            const FTransform Keys[]={Rest,Toss,Approach,Above,Worn};
            const float Times[]={13.f,14.4f,15.5f,17.f,18.f};
            Held=Journey(Keys,Times,5,T);
        }
        else if(T>=23.8 && T<29)
        {
            const FTransform Keys[]={Worn,Above,Approach,Toss,Rest};
            const float Times[]={23.8f,25.f,26.8f,27.7f,29.f};
            Held=Journey(Keys,Times,5,T);
        }
        P.Rotation=Held.GetRotation();
        P.Wrist=Held.GetLocation()-P.Rotation.RotateVector(HatOffset());
        return P;
    }
    FTransform HatTransform() const
    { return FTransform(Rotation,Wrist+Rotation.RotateVector(HatOffset())); }
};
