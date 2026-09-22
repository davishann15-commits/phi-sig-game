#include "SeniorDouliAnim.h"
#include "SeniorDouliIdle.h"
#include "SeniorSelectionRoom.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "TwoBoneIK.h"

namespace {
struct FDouliProxy : FAnimInstanceProxy
{
    FAnimNode_SequencePlayer_Standalone Player;
    bool Equipped=false, FirstPerson=false; float Motion=0, Flight=0;
    bool Lobby=false; float LobbyTime=0;
    bool SelectionRoom=false; float RoomTime=0;
    explicit FDouliProxy(UAnimInstance* I):FAnimInstanceProxy(I) { Player.SetLoopAnimation(true); }
    virtual void PreUpdate(UAnimInstance* I,float D) override
    {
        FAnimInstanceProxy::PreUpdate(I,D);
        const auto* A=CastChecked<USeniorDouliAnim>(I);
        Player.SetSequence(A->BaseSequence); Player.SetPlayRate(A->Speed>5 ? FMath::Clamp(A->Speed/155.f,.65f,2.1f) : 1.f);
        Equipped=A->bEquipped; FirstPerson=A->bFirstPerson; Motion=A->Motion; Flight=A->FlightBlend;
        Lobby=A->bLobbyIdle; LobbyTime=A->LobbyTime;
        SelectionRoom=A->bSelectionRoom; RoomTime=A->SelectionRoomTime;
    }
    virtual FAnimNode_Base* GetCustomRootNode() override { return &Player; }
    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& N) override { N.Add(&Player); }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        Player.Evaluate_AnyThread(Output);
        if (!Equipped && !SelectionRoom) return true;
        const FBoneContainer& Bones=Output.Pose.GetBoneContainer();
        auto Index=[&](FName Name) { return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bones.GetReferenceSkeleton().FindBoneIndex(Name))); };
        auto Component=[&](FCompactPoseBoneIndex B) {
            FTransform T=Output.Pose[B];
            for(FCompactPoseBoneIndex P=Bones.GetParentBoneIndex(B);P.IsValid();P=Bones.GetParentBoneIndex(P)) T=T*Output.Pose[P];
            return T;
        };
        if(Lobby && !FirstPerson)
        {
            // Unround the idle's upper back without moving the feet or changing
            // gameplay locomotion. Component-space pitch is independent of rig axes.
            for(const TCHAR* Name : {TEXT("spine_02"),TEXT("spine_04")})
            {
                const auto B=Index(Name); if(!B.IsValid())continue;
                FTransform T=Component(B);
                T.SetRotation((FQuat(FVector(1,0,0),FMath::DegreesToRadians(3.f))*T.GetRotation()).GetNormalized());
                Output.Pose[B]=T.GetRelativeTransform(Component(Bones.GetParentBoneIndex(B)));
            }
            const auto Head=Index(TEXT("head"));
            if(Head.IsValid())
            {
                FTransform T=Component(Head);
                T.SetRotation((FQuat(FVector(1,0,0),FMath::DegreesToRadians(-4.f))*T.GetRotation()).GetNormalized());
                Output.Pose[Head]=T.GetRelativeTransform(Component(Bones.GetParentBoneIndex(Head)));
            }
        }
        if(SelectionRoom && Lobby && !FirstPerson)
        {
            // Component-space axes keep the head turn independent of bone orientation.
            auto Rotate=[&](const TCHAR* Name,const FQuat& Delta)
            {
                const auto B=Index(Name); if(!B.IsValid())return;
                FTransform T=Component(B); T.SetRotation((Delta*T.GetRotation()).GetNormalized());
                const auto Parent=Bones.GetParentBoneIndex(B);
                Output.Pose[B]=T.GetRelativeTransform(Component(Parent));
            };
            const float Look=FSeniorSelectionRoom::Pulse(RoomTime,3.3f,7.f);
            const float Behind=FSeniorSelectionRoom::Pulse(RoomTime,16.f,20.f);
            Rotate(TEXT("head"),FQuat(FVector(1,0,0),FMath::DegreesToRadians(18.f*Look))
                *FQuat(FVector(0,0,1),FMath::DegreesToRadians(38.f*Behind)));
            Rotate(TEXT("spine_03"),FQuat(FVector(0,0,1),FMath::DegreesToRadians(6.f*Behind)));
            // A small toe scuff: the other foot stays planted. IK preserves leg length.
            const float Scuff=FSeniorSelectionRoom::Pulse(RoomTime,28.f,31.f);
            if(Scuff>0)
            {
                const auto U=Index(TEXT("thigh_r")),L=Index(TEXT("calf_r")),F=Index(TEXT("foot_r"));
                if(U.IsValid() && L.IsValid() && F.IsValid())
                {
                    FTransform A=Component(U),B=Component(L),C=Component(F);
                    const FVector Target=C.GetLocation()+FVector(1.5f,5.f,.7f)*Scuff;
                    AnimationCore::SolveTwoBoneIK(A,B,C,B.GetLocation()+FVector(0,35,0),Target,false,1.,1.);
                    Output.Pose[U]=A.GetRelativeTransform(Component(Bones.GetParentBoneIndex(U)));
                    Output.Pose[L]=B.GetRelativeTransform(A); Output.Pose[F]=C.GetRelativeTransform(B);
                }
            }
        }
        if(!Equipped) { Output.Pose.NormalizeRotations(); return true; }
        // A reference-guided rim grip. Native body is +Y forward; legacy FP arms are +X.
        for (bool Right : {true,false})
        {
            // Only the lobby's passing section animates the left arm. Gameplay
            // retains its approved one-handed pose and separate first-person arms.
            FSeniorDouliIdle IdlePose=FSeniorDouliIdle::At(LobbyTime);
            if (!Right && !FirstPerson && (!Lobby || IdlePose.LeftBlend<=0)) continue;
            const float ArmBlend=Right?1.f:IdlePose.LeftBlend;
            if(Lobby && !Right)
            {
                IdlePose.Wrist=IdlePose.LeftWrist; IdlePose.Rotation=IdlePose.LeftRotation;
                IdlePose.Grip=IdlePose.LeftGrip*ArmBlend; IdlePose.HeadFollow=0;
            }
            const auto U=Index(Right?TEXT("upperarm_r"):TEXT("upperarm_l"));
            const auto L=Index(Right?TEXT("lowerarm_r"):TEXT("lowerarm_l"));
            const auto H=Index(Right?TEXT("hand_r"):TEXT("hand_l"));
            if(!U.IsValid() || !L.IsValid() || !H.IsValid()) continue;
            if(Lobby && Right)
            {
                // Let the shoulder girdle participate in the overhead reach,
                // rather than forcing the whole lift through a fixed upper arm.
                const auto Clavicle=Index(TEXT("clavicle_r"));
                const float Lift=FSeniorDouliIdle::Ease((IdlePose.Wrist.Z-140.f)/35.f);
                if(Clavicle.IsValid() && Lift>0)
                {
                    FTransform C=Component(Clavicle);
                    C.SetRotation((FQuat(FVector(0,1,0),FMath::DegreesToRadians(8.f*Lift))*C.GetRotation()).GetNormalized());
                    Output.Pose[Clavicle]=C.GetRelativeTransform(Component(Bones.GetParentBoneIndex(Clavicle)));
                }
            }
            const FTransform BaseUpper=Output.Pose[U],BaseLower=Output.Pose[L],BaseHand=Output.Pose[H];
            FTransform Upper=Component(U), Lower=Component(L), Hand=Component(H);
            const auto Middle=Index(Right?TEXT("middle_01_r"):TEXT("middle_01_l"));
            const FVector FingerAxis=Middle.IsValid() ? Hand.InverseTransformVectorNoScale(Component(Middle).GetLocation()-Hand.GetLocation()).GetSafeNormal() : FVector(1,0,0);
            FVector Target;
            FVector Pole;
            if (FirstPerson)
            {
                Target=Right?FVector(42,35,124):FVector(14,-35,100);
                Target+=Right?FVector(15*Motion,30*FMath::Min(Motion,0.f)+18*FMath::Max(Motion,0.f),12*FMath::Abs(Motion)):FVector(0,0,-6*FMath::Abs(Motion));
                Target+=Right?FVector(-8,5,7)*Flight:FVector(-6,-4,-18)*Flight;
                Pole=Right?FVector(8,65,120):FVector(8,-65,115);
            }
            else
            {
                Target=Right?FVector(-28,38,108):FVector(18,38,108);
                Target+=Right?FVector(-25*Motion,18*Motion,14*FMath::Abs(Motion)):FVector(9*FMath::Abs(Motion),-15*FMath::Abs(Motion),-7*FMath::Abs(Motion));
                Target+=Right?FVector(-3,-4,8)*Flight:FVector(6,-24,-20)*Flight;
                Pole=Right?FVector(-52,8,107):FVector(52,7,112);
            }
            if (Lobby)
            {
                const auto Head=Index(TEXT("head"));
                if(Head.IsValid() && IdlePose.HeadFollow>0)
                {
                    const FTransform Worn=FSeniorDouliIdle::HeadHat(Bones.GetReferenceSkeleton())*Component(Head);
                    IdlePose.Wrist+=(Worn.GetLocation()-FSeniorDouliIdle::WornLocation())*IdlePose.HeadFollow;
                    const FQuat FollowRotation=FQuat::Slerp(IdlePose.Rotation,Worn.GetRotation(),IdlePose.HeadFollow);
                    IdlePose.Wrist+=IdlePose.Rotation.RotateVector(FSeniorDouliIdle::HatOffset())-FollowRotation.RotateVector(FSeniorDouliIdle::HatOffset());
                    IdlePose.Rotation=FollowRotation;
                }
                Target=IdlePose.Wrist;
                // Keep the elbow below the shoulder for forward holds, then
                // smoothly open outward only as the hand rises to the head.
                // Do not roll the lower-arm bone: that twists the elbow skin.
                Pole=FMath::Lerp(FVector(-34,-12,100),FVector(-52,4,145),FSeniorDouliIdle::Ease((Target.Z-125)/50.f));
                if(!Right)Pole.X=-Pole.X;
            }
            AnimationCore::SolveTwoBoneIK(Upper,Lower,Hand,Pole,Target,false,1.,1.);
            FVector FingerDirection=FirstPerson ? FVector(.8,Right?-.6:.6,-.12) : FVector(Right?1:-1,.12,-.12);
            Hand.SetRotation((FQuat::FindBetweenNormals(Hand.TransformVectorNoScale(FingerAxis).GetSafeNormal(),FingerDirection.GetSafeNormal())*Hand.GetRotation()).GetNormalized());
            if (Lobby) Hand.SetRotation((IdlePose.Rotation
                *FSeniorDouliIdle::PalmFrame(Right)*FSeniorDouliIdle::HandFrame(Bones.GetReferenceSkeleton(),Right).Inverse()).GetNormalized());
            const auto Parent=Bones.GetParentBoneIndex(U);
            Output.Pose[U]=Upper.GetRelativeTransform(Component(Parent));
            Output.Pose[L]=Lower.GetRelativeTransform(Upper);
            Output.Pose[H]=Hand.GetRelativeTransform(Lower);
            if(Lobby && !Right)
            {
                FTransform BlendedArm;
                BlendedArm.Blend(BaseUpper,Output.Pose[U],ArmBlend); Output.Pose[U]=BlendedArm;
                BlendedArm.Blend(BaseLower,Output.Pose[L],ArmBlend); Output.Pose[L]=BlendedArm;
                BlendedArm.Blend(BaseHand,Output.Pose[H],ArmBlend); Output.Pose[H]=BlendedArm;
            }
            if (Lobby)
            {
                const FQuat HatRotation=IdlePose.Rotation;
                const FQuat PalmRotation=HatRotation*FSeniorDouliIdle::PalmFrame(Right);
                const FVector Forward=PalmRotation.GetAxisX(), Up=PalmRotation.GetAxisZ();
                const FVector BendAxis=FVector::CrossProduct(Forward,-Up).GetSafeNormal();
                // Fingers curl below the thin brim; the thumb opposes them above.
                for (const TCHAR* Finger:{TEXT("index"),TEXT("middle"),TEXT("ring"),TEXT("pinky")})
                    for(int32 Joint=1;Joint<=3;++Joint)
                    {
                        const auto J=Index(*FString::Printf(TEXT("%s_0%d_%s"),Finger,Joint,Right?TEXT("r"):TEXT("l")));
                        if(!J.IsValid())continue;
                        const auto P=Bones.GetParentBoneIndex(J);
                        const FQuat ParentRotation=Component(P).GetRotation();
                        const FVector LocalAxis=ParentRotation.Inverse().RotateVector(BendAxis);
                        const float Angle=Joint==1?7.f:(Joint==2?45.f:25.f);
                        Output.Pose[J].SetRotation((FQuat(LocalAxis,FMath::DegreesToRadians(Angle*IdlePose.Grip))*Output.Pose[J].GetRotation()).GetNormalized());
                    }
                const auto T1=Index(Right?TEXT("thumb_01_r"):TEXT("thumb_01_l")),T2=Index(Right?TEXT("thumb_02_r"):TEXT("thumb_02_l")),T3=Index(Right?TEXT("thumb_03_r"):TEXT("thumb_03_l"));
                if(T1.IsValid() && T2.IsValid() && T3.IsValid())
                {
                    FTransform A=Component(T1),B=Component(T2),C=Component(T3);
                    const FVector Tip=Hand.GetLocation()+HatRotation.RotateVector(FVector(5,Right?8:-8,1));
                    const FVector ThumbPole=Hand.GetLocation()+HatRotation.RotateVector(FVector(-5,Right?2:-2,5));
                    AnimationCore::SolveTwoBoneIK(A,B,C,ThumbPole,Tip,false,1.,1.);
                    const FVector ThumbAxis=(C.GetLocation()-B.GetLocation()).GetSafeNormal();
                    const FVector ThumbPad=Hand.GetLocation()+HatRotation.RotateVector(FVector(6.5,Right?11:-11,-1));
                    C.SetRotation((FQuat::FindBetweenNormals(ThumbAxis,(ThumbPad-C.GetLocation()).GetSafeNormal())*C.GetRotation()).GetNormalized());
                    FTransform Blended;
                    Blended.Blend(Output.Pose[T1],A.GetRelativeTransform(Component(Bones.GetParentBoneIndex(T1))),IdlePose.Grip); Output.Pose[T1]=Blended;
                    Blended.Blend(Output.Pose[T2],B.GetRelativeTransform(A),IdlePose.Grip); Output.Pose[T2]=Blended;
                    Blended.Blend(Output.Pose[T3],C.GetRelativeTransform(B),IdlePose.Grip); Output.Pose[T3]=Blended;
                }
            }
        }
        Output.Pose.NormalizeRotations();
        return true;
    }
};
}
FAnimInstanceProxy* USeniorDouliAnim::CreateAnimInstanceProxy() { return new FDouliProxy(this); }
void USeniorDouliAnim::DestroyAnimInstanceProxy(FAnimInstanceProxy* P) { delete P; }
