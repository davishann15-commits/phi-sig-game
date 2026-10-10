#include "BoywickAnimInstance.h"
#include "SeniorDouliIdle.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "TwoBoneIK.h"

namespace
{
struct FBoywickBodyProxy : FAnimInstanceProxy
{
    FAnimNode_SequencePlayer_Standalone Idle, Walk, Jog;
    FAnimNode_TwoWayBlend Walking, Locomotion;
    float Talk = 0.f, Angry = 0.f, Clock = 0.f, Yaw = 0.f, Pitch = 0.f;
    explicit FBoywickBodyProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
    {
        Idle.SetLoopAnimation(true); Walk.SetLoopAnimation(true); Jog.SetLoopAnimation(true);
        Walking.A.SetLinkNode(&Idle); Walking.B.SetLinkNode(&Walk);
        Locomotion.A.SetLinkNode(&Walking); Locomotion.B.SetLinkNode(&Jog);
    }
    virtual void PreUpdate(UAnimInstance* Instance, float Delta) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, Delta);
        const auto* A = CastChecked<UBoywickAnimInstance>(Instance);
        Idle.SetSequence(A->Idle); Walk.SetSequence(A->Walk); Jog.SetSequence(A->Jog);
        // Retargeted footfall speed is matched to the shorter legs; starting
        // and stopping blend over several frames rather than resetting a clip.
        Walk.SetPlayRate(FMath::Clamp(A->Speed / 135.f, .55f, 1.65f));
        Jog.SetPlayRate(FMath::Clamp(A->Speed / 310.f, .75f, 1.35f));
        Walking.Alpha = FMath::FInterpTo(Walking.Alpha, FMath::Clamp(A->Speed / 45.f, 0.f, 1.f), Delta, 7.f);
        Locomotion.Alpha = FMath::FInterpTo(Locomotion.Alpha, FMath::Clamp((A->Speed - 175.f) / 110.f, 0.f, 1.f), Delta, 5.f);
        Talk = A->Talking; Angry = A->Anger; Clock = A->Time; Yaw = A->HeadYaw; Pitch = A->HeadPitch;
    }
    virtual FAnimNode_Base* GetCustomRootNode() override { return &Locomotion; }
    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& Nodes) override
    {
        Nodes.Add(&Idle); Nodes.Add(&Walk); Nodes.Add(&Jog); Nodes.Add(&Walking); Nodes.Add(&Locomotion);
    }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        Locomotion.Evaluate_AnyThread(Output);
        const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
        auto Index = [&](FName Name) { return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bones.GetReferenceSkeleton().FindBoneIndex(Name))); };
        auto Component = [&](FCompactPoseBoneIndex Bone)
        {
            FTransform T = Output.Pose[Bone];
            for (auto P = Bones.GetParentBoneIndex(Bone); P.IsValid(); P = Bones.GetParentBoneIndex(P)) T = T * Output.Pose[P];
            return T;
        };
        const FVector Forward(0, 1, 0), Right(-1, 0, 0), Up(0, 0, 1);
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const bool bRight = Side == 0;
            const float Sign = bRight ? 1.f : -1.f;
            const auto U = Index(bRight ? TEXT("upperarm_r") : TEXT("upperarm_l"));
            const auto L = Index(bRight ? TEXT("lowerarm_r") : TEXT("lowerarm_l"));
            const auto H = Index(bRight ? TEXT("hand_r") : TEXT("hand_l"));
            if (!U.IsValid() || !L.IsValid() || !H.IsValid()) continue;
            const float Beat = .5f + .5f * FMath::Sin(Clock * 1.6f + Side * 2.1f);
            const float Gesture = Talk * (.50f + .35f * Beat);
            const float Blend = FMath::Max(Gesture, Angry * .72f);
            if (Blend < .001f) continue;
            FTransform Upper = Component(U), Lower = Component(L), Hand = Component(H);
            const FTransform BaseU = Output.Pose[U], BaseL = Output.Pose[L], BaseH = Output.Pose[H];
            const float Length = FVector::Distance(Upper.GetLocation(), Lower.GetLocation()) + FVector::Distance(Lower.GetLocation(), Hand.GetLocation());
            // Relaxed conversational hands remain in front of the waist/chest,
            // with varied left/right emphasis instead of a continuous wave.
            const FVector Target = Upper.GetLocation() + Forward * Length * (.25f + .14f * Beat + .12f * Angry)
                + Right * Sign * Length * (.18f + .09f * Beat) - Up * Length * (.57f - .15f * Beat - .29f * Angry);
            const FVector Pole = Upper.GetLocation() - Forward * Length * .08f + Right * Sign * Length * .45f - Up * Length * .30f;
            AnimationCore::SolveTwoBoneIK(Upper, Lower, Hand, Pole, Target, false, 1., 1.);
            FTransform Result;
            Result.Blend(BaseU, Upper.GetRelativeTransform(Component(Bones.GetParentBoneIndex(U))), Blend); Output.Pose[U] = Result;
            Result.Blend(BaseL, Lower.GetRelativeTransform(Upper), Blend); Output.Pose[L] = Result;
            Result.Blend(BaseH, Hand.GetRelativeTransform(Lower), Blend); Output.Pose[H] = Result;
        }
        for (const TCHAR* Name : {TEXT("neck_01"), TEXT("head")})
        {
            const auto Bone = Index(Name);
            if (!Bone.IsValid()) continue;
            const float Share = FCString::Strcmp(Name, TEXT("head")) == 0 ? .65f : .35f;
            const float Nod = Talk * (FMath::Sin(Clock * 1.9f) * 2.4f + FMath::Sin(Clock * .73f) * 1.2f);
            FTransform CS = Component(Bone);
            const FQuat Rotation = FQuat(Up, FMath::DegreesToRadians(-Yaw * Share))
                * FQuat(FVector(1,0,0), FMath::DegreesToRadians((Pitch + Nod - Angry * 4.f) * Share));
            CS.SetRotation((Rotation * CS.GetRotation()).GetNormalized());
            Output.Pose[Bone] = CS.GetRelativeTransform(Component(Bones.GetParentBoneIndex(Bone)));
        }
        Output.Pose.NormalizeRotations();
        return true;
    }
};

struct FBoywickFaceProxy : FAnimInstanceProxy
{
    FAnimNode_CopyPoseFromMesh Pose;
    float Talk = 0.f, Angry = 0.f, Clock = 0.f, Speech = 0.f, Sleep = 0.f;
    explicit FBoywickFaceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
    {
        Pose.bUseAttachedParent = true; Pose.bUseMeshPose = true; Pose.bCopyCurves = true;
    }
    virtual void Initialize(UAnimInstance* Instance) override
    {
        if (USkeletalMeshComponent* Mesh = Instance->GetSkelMeshComponent())
        {
            TInlineComponentArray<USkeletalMeshComponent*> Meshes(Mesh->GetOwner());
            for (auto* Other : Meshes) if (Other->GetFName() == TEXT("Body")) Pose.SourceMeshComponent = Other;
        }
        FAnimInstanceProxy::Initialize(Instance);
    }
    virtual void PreUpdate(UAnimInstance* Instance, float Delta) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, Delta);
        const auto* A = CastChecked<UBoywickFaceAnimInstance>(Instance);
        Talk = A->Talking; Angry = A->Anger; Clock = A->Time; Speech = A->SpeechPhase; Sleep = A->Sleep;
    }
    virtual FAnimNode_Base* GetCustomRootNode() override { return &Pose; }
    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& Nodes) override { Nodes.Add(&Pose); }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        Pose.Evaluate_AnyThread(Output);
        const float BlinkPhase = FMath::Fmod(Clock, 4.7f);
        const float Blink = FMath::Max(Sleep, FMath::Exp(-FMath::Square((BlinkPhase - .17f) / .075f)));
        const float Syllable = FMath::Max(0.f, FMath::Sin(Speech * 8.9f) * .65f + FMath::Sin(Speech * 15.7f + .6f) * .35f);
        const float Pause = FMath::Fmod(Speech, 3.1f) > 2.6f ? .18f : 1.f;
        Output.Curve.Set(TEXT("CTRL_expressions_eyeBlinkL"), Blink);
        Output.Curve.Set(TEXT("CTRL_expressions_eyeBlinkR"), Blink);
        Output.Curve.Set(TEXT("CTRL_expressions_jawOpen"), Talk * (.025f + .23f * Syllable) * Pause);
        Output.Curve.Set(TEXT("CTRL_expressions_mouthFunnelDL"), Talk * .06f * (1.f - Syllable));
        Output.Curve.Set(TEXT("CTRL_expressions_mouthFunnelDR"), Talk * .06f * (1.f - Syllable));
        Output.Curve.Set(TEXT("CTRL_expressions_mouthCornerPullL"), .08f * (1.f - Angry) + Talk * .07f);
        Output.Curve.Set(TEXT("CTRL_expressions_mouthCornerPullR"), .08f * (1.f - Angry) + Talk * .07f);
        Output.Curve.Set(TEXT("CTRL_expressions_browDownL"), Angry * .5f);
        Output.Curve.Set(TEXT("CTRL_expressions_browDownR"), Angry * .5f);
        Output.Curve.Set(TEXT("CTRL_expressions_browRaiseInL"), Talk * .10f * (.5f + .5f * FMath::Sin(Speech * .8f)));
        Output.Curve.Set(TEXT("CTRL_expressions_browRaiseInR"), Talk * .10f * (.5f + .5f * FMath::Sin(Speech * .8f)));
        return true;
    }
};
}
FAnimInstanceProxy* UBoywickAnimInstance::CreateAnimInstanceProxy() { return new FBoywickBodyProxy(this); }
void UBoywickAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
FAnimInstanceProxy* UBoywickFaceAnimInstance::CreateAnimInstanceProxy() { return new FBoywickFaceProxy(this); }
void UBoywickFaceAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
