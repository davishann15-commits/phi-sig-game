#include "SeniorDouliAnim.h"
#include "SeniorDouliIdle.h"
#include "SeniorSelectionRoom.h"
#include "SeniorPigHoldPose.h"
#include "SeniorFurnitureHoldPose.h"
#include "HouseFurnitureInteractionComponent.h"
#include "HouseRoamingPig.h"
#include "Components/SkeletalMeshComponent.h"
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
    float PigBlend=0, PigGrip=0;
    FVector PigTarget=FVector::ZeroVector, PigAxis=FVector::UpVector, PigForward=FVector(0,-1,0);
    float FurnitureBlend=0, FurnitureGrip=0;
    bool HeavyFurniture=false, TwoFurnitureHands=true;
    float FurnitureCrouch=0.f;
    FVector FurniturePoint[2],FurnitureNormal[2],FurnitureUp=FVector::UpVector;
    float FurnitureLean=0.f;
    FVector FurnitureFinger[2]; bool bFurnitureFinger[2]={false,false};
    ESSOCarrySize FurnitureSize=ESSOCarrySize::Medium;
    float FurnitureStrain=0.f, FurnitureWindup=0.f, FurnitureFollow=0.f, FurnitureTwist=0.f, FurnitureBrace=0.f;
    float CrawlBlend=0.f, CrawlPhase=0.f, CrawlSpeed=0.f;
    explicit FDouliProxy(UAnimInstance* I):FAnimInstanceProxy(I) { Player.SetLoopAnimation(true); }
    virtual void PreUpdate(UAnimInstance* I,float D) override
    {
        FAnimInstanceProxy::PreUpdate(I,D);
        const auto* A=CastChecked<USeniorDouliAnim>(I);
        Player.SetSequence(A->BaseSequence); Player.SetPlayRate(A->Speed>5 ? FMath::Clamp(A->Speed/155.f,.65f,2.1f) : 1.f);
        Equipped=A->bEquipped; FirstPerson=A->bFirstPerson; Motion=A->Motion; Flight=A->FlightBlend;
        CrawlBlend=A->CrawlBlend; CrawlPhase=A->CrawlPhase; CrawlSpeed=A->CrawlSpeed;
        Lobby=A->bLobbyIdle; LobbyTime=A->LobbyTime;
        SelectionRoom=A->bSelectionRoom; RoomTime=A->SelectionRoomTime;
        PigBlend=A->PigCarryBlend; PigGrip=A->PigGripBlend;
        PigTarget=A->PigGripTarget; PigAxis=A->PigGripDirection; PigForward=A->PigHandForward;
        FurnitureBlend=0; FurnitureGrip=0; HeavyFurniture=false;
        FurnitureWindup=0.f; FurnitureFollow=0.f;
        float DesiredLean=0.f, DesiredStrain=0.f;
        if(IsValid(A->FurnitureCarryTarget) && GetSkelMeshComponent())
        {
            const FTransform Mesh=GetSkelMeshComponent()->GetComponentTransform();
            FurnitureBlend=A->FurnitureCarryTarget->GetCarryPoseBlend();
            FurnitureGrip=A->FurnitureCarryTarget->GetFingerGripBlend();
            DesiredLean=A->FurnitureCarryTarget->GetPickupLean();
            HeavyFurniture=A->FurnitureCarryTarget->IsDraggingHeavyFurniture();
            TwoFurnitureHands=A->FurnitureCarryTarget->UsesTwoHands();
            FurnitureSize=A->FurnitureCarryTarget->GetCarrySize();
            DesiredStrain=A->FurnitureCarryTarget->GetStrain();
            FurnitureWindup=A->FurnitureCarryTarget->GetThrowWindup();
            FurnitureFollow=A->FurnitureCarryTarget->GetThrowFollowThrough();
            FurnitureUp=Mesh.InverseTransformVectorNoScale(FVector::UpVector).GetSafeNormal();
            for(int32 Hand=0;Hand<2;++Hand)
            {
                FVector Point,Normal,Fingers;
                A->FurnitureCarryTarget->GetHandContact(Hand==0,Point,Normal);
                FurniturePoint[Hand]=Mesh.InverseTransformPosition(Point);
                FurnitureNormal[Hand]=Mesh.InverseTransformVectorNoScale(Normal).GetSafeNormal();
                bFurnitureFinger[Hand]=A->FurnitureCarryTarget->GetHandFingerDirection(Hand==0,Fingers);
                FurnitureFinger[Hand]=bFurnitureFinger[Hand]?Mesh.InverseTransformVectorNoScale(Fingers).GetSafeNormal():FVector::ZeroVector;
            }
        }
        FurnitureLean=FMath::FInterpTo(FurnitureLean,DesiredLean,D,8.f);
        FurnitureStrain=FMath::FInterpTo(FurnitureStrain,DesiredStrain,D,6.f);
        // Right-handed throw: the torso turns back with the wind-up and
        // unwinds through the release. Big pieces twist far less.
        const float TwistScale=FurnitureSize==ESSOCarrySize::Small?1.f:(FurnitureSize==ESSOCarrySize::Medium?.45f:.25f);
        FurnitureTwist=FMath::FInterpTo(FurnitureTwist,(20.f*FurnitureWindup-24.f*FurnitureFollow)*TwistScale,D,14.f);
        FurnitureBrace=FMath::FInterpTo(FurnitureBrace,FurnitureBlend>.001f && FurnitureSize==ESSOCarrySize::Large?DesiredStrain:0.f,D,6.f);
        float DesiredCrouch=0.f;
        if(!FirstPerson && FurnitureBlend>.001f && GetSkelMeshComponent()->GetSkeletalMeshAsset())
        {
            const float ShoulderZ=FSeniorDouliIdle::RefBone(GetSkelMeshComponent()->GetSkeletalMeshAsset()->GetRefSkeleton(),TEXT("upperarm_r")).GetLocation().Z;
            const float ContactZ=(FurniturePoint[0].Z+FurniturePoint[1].Z)*.5f;
            DesiredCrouch=FMath::Clamp(ShoulderZ-ContactZ-36.f+10.f*FurnitureBrace,0.f,75.f)*FurnitureBlend;
        }
        FurnitureCrouch=FMath::FInterpTo(FurnitureCrouch,DesiredCrouch,D,8.f);
        if(IsValid(A->PigTargetActor) && GetSkelMeshComponent())
        {
            const FTransform Mesh=GetSkelMeshComponent()->GetComponentTransform();
            PigTarget=Mesh.InverseTransformPosition(A->PigTargetActor->GetCarryGripPoint());
            PigAxis=Mesh.InverseTransformVectorNoScale(A->PigTargetActor->GetCarryGripDirection()).GetSafeNormal();
        }
    }
    virtual FAnimNode_Base* GetCustomRootNode() override { return &Player; }
    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& N) override { N.Add(&Player); }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        Player.Evaluate_AnyThread(Output);
        if (!Equipped && !SelectionRoom && PigBlend<=.001f && FurnitureBlend<=.001f && CrawlBlend<=.001f) return true;
        const FBoneContainer& Bones=Output.Pose.GetBoneContainer();
        auto Index=[&](FName Name) { return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bones.GetReferenceSkeleton().FindBoneIndex(Name))); };
        auto Component=[&](FCompactPoseBoneIndex B) {
            FTransform T=Output.Pose[B];
            for(FCompactPoseBoneIndex P=Bones.GetParentBoneIndex(B);P.IsValid();P=Bones.GetParentBoneIndex(P)) T=T*Output.Pose[P];
            return T;
        };
        if(CrawlBlend>.001f && FirstPerson)
        {
            // The owner sees a low, alternating forearm reach. This mesh is
            // camera-mounted and uses +X forward; it has no visible legs.
            const float CrawlMotion=FMath::Clamp(CrawlSpeed/45.f,0.f,1.f);
            for(int32 Side=0;Side<2;++Side)
            {
                const bool bRight=Side==0;
                const float Sign=bRight?1.f:-1.f;
                const float Phase=CrawlPhase+(bRight?0.f:PI);
                const float Stroke=CrawlMotion*FMath::Sin(Phase);
                const float Lift=CrawlMotion*FMath::Max(0.f,-FMath::Cos(Phase));
                const auto Shoulder=Index(bRight?TEXT("upperarm_r"):TEXT("upperarm_l"));
                const auto Elbow=Index(bRight?TEXT("lowerarm_r"):TEXT("lowerarm_l"));
                const auto Wrist=Index(bRight?TEXT("hand_r"):TEXT("hand_l"));
                if(!Shoulder.IsValid() || !Elbow.IsValid() || !Wrist.IsValid())continue;
                const FTransform BaseA=Output.Pose[Shoulder],BaseB=Output.Pose[Elbow],BaseC=Output.Pose[Wrist];
                FTransform A=Component(Shoulder),B=Component(Elbow),C=Component(Wrist);
                const float Length=(A.GetLocation()-B.GetLocation()).Size()+(B.GetLocation()-C.GetLocation()).Size();
                const FVector Target=A.GetLocation()+FVector(Length*(.57f+.13f*Stroke),
                    Sign*Length*.07f,-Length*(.33f-.08f*Lift));
                const FVector Pole=A.GetLocation()+FVector(Length*.23f,
                    Sign*Length*.42f,-Length*.38f);
                AnimationCore::SolveTwoBoneIK(A,B,C,Pole,Target,false,1.,1.);
                FTransform Blend;
                Blend.Blend(BaseA,A.GetRelativeTransform(Component(Bones.GetParentBoneIndex(Shoulder))),CrawlBlend);Output.Pose[Shoulder]=Blend;
                Blend.Blend(BaseB,B.GetRelativeTransform(A),CrawlBlend);Output.Pose[Elbow]=Blend;
                Blend.Blend(BaseC,C.GetRelativeTransform(B),CrawlBlend);Output.Pose[Wrist]=Blend;
            }
            Output.Pose.NormalizeRotations();
            return true;
        }
        if(CrawlBlend>.001f)
        {
            // Both the Cobble bodies and the three detailed bodies have these
            // limb chains. The toe direction supplies each rig's anatomical
            // forward axis, so the same reach works for +X and +Y skeletons.
            const auto FootL=Index(TEXT("foot_l")), ToeL=Index(TEXT("ball_l"));
            FVector Forward=FootL.IsValid() && ToeL.IsValid()
                ?Component(ToeL).GetLocation()-Component(FootL).GetLocation():FVector::ForwardVector;
            Forward.Z=0.f;
            Forward=Forward.GetSafeNormal();
            if(Forward.IsNearlyZero()) Forward=FVector::ForwardVector;
            const FVector Up=FVector::UpVector;
            const FVector Right=FVector::CrossProduct(Up,Forward).GetSafeNormal();
            const float CrawlMotion=FMath::Clamp(CrawlSpeed/45.f,0.f,1.f);
            for(int32 Side=0;Side<2;++Side)
            {
                const bool bRight=Side==0;
                const float Sign=bRight?1.f:-1.f;
                const float Phase=CrawlPhase+(bRight?0.f:PI);
                const float Stroke=CrawlMotion*FMath::Sin(Phase);
                const float Lift=CrawlMotion*FMath::Max(0.f,-FMath::Cos(Phase));
                const auto Shoulder=Index(bRight?TEXT("upperarm_r"):TEXT("upperarm_l"));
                const auto Elbow=Index(bRight?TEXT("lowerarm_r"):TEXT("lowerarm_l"));
                const auto Wrist=Index(bRight?TEXT("hand_r"):TEXT("hand_l"));
                if(Shoulder.IsValid() && Elbow.IsValid() && Wrist.IsValid())
                {
                    const FTransform BaseA=Output.Pose[Shoulder],BaseB=Output.Pose[Elbow],BaseC=Output.Pose[Wrist];
                    FTransform A=Component(Shoulder),B=Component(Elbow),C=Component(Wrist);
                    const float Length=(A.GetLocation()-B.GetLocation()).Size()+(B.GetLocation()-C.GetLocation()).Size();
                    const FVector Target=A.GetLocation()+Up*Length*(.49f+.16f*Stroke)
                        +Forward*Length*(.34f-.08f*Lift)+Right*Sign*Length*.06f;
                    const FVector Pole=A.GetLocation()+Up*Length*(.11f+.10f*Stroke)
                        +Forward*Length*.45f+Right*Sign*Length*.23f;
                    AnimationCore::SolveTwoBoneIK(A,B,C,Pole,Target,false,1.,1.);
                    FTransform Blend;
                    Blend.Blend(BaseA,A.GetRelativeTransform(Component(Bones.GetParentBoneIndex(Shoulder))),CrawlBlend);Output.Pose[Shoulder]=Blend;
                    Blend.Blend(BaseB,B.GetRelativeTransform(A),CrawlBlend);Output.Pose[Elbow]=Blend;
                    Blend.Blend(BaseC,C.GetRelativeTransform(B),CrawlBlend);Output.Pose[Wrist]=Blend;
                }
                const auto Hip=Index(bRight?TEXT("thigh_r"):TEXT("thigh_l"));
                const auto Knee=Index(bRight?TEXT("calf_r"):TEXT("calf_l"));
                const auto Ankle=Index(bRight?TEXT("foot_r"):TEXT("foot_l"));
                if(Hip.IsValid() && Knee.IsValid() && Ankle.IsValid())
                {
                    const FTransform BaseA=Output.Pose[Hip],BaseB=Output.Pose[Knee],BaseC=Output.Pose[Ankle];
                    FTransform A=Component(Hip),B=Component(Knee),C=Component(Ankle);
                    const float Length=(A.GetLocation()-B.GetLocation()).Size()+(B.GetLocation()-C.GetLocation()).Size();
                    // The leg opposite the reaching arm tucks forward while
                    // this leg pushes back. The knee stays bent toward the floor.
                    const FVector Target=A.GetLocation()-Up*Length*(.63f+.12f*Stroke)
                        +Forward*Length*(.05f-.07f*Lift)+Right*Sign*Length*.04f;
                    const FVector Pole=A.GetLocation()-Up*Length*(.29f+.05f*Stroke)
                        +Forward*Length*.30f+Right*Sign*Length*.14f;
                    AnimationCore::SolveTwoBoneIK(A,B,C,Pole,Target,false,1.,1.);
                    FTransform Blend;
                    Blend.Blend(BaseA,A.GetRelativeTransform(Component(Bones.GetParentBoneIndex(Hip))),CrawlBlend);Output.Pose[Hip]=Blend;
                    Blend.Blend(BaseB,B.GetRelativeTransform(A),CrawlBlend);Output.Pose[Knee]=Blend;
                    Blend.Blend(BaseC,C.GetRelativeTransform(B),CrawlBlend);Output.Pose[Ankle]=Blend;
                }
            }
            // Turn the face toward the duct ahead instead of into its floor.
            const FQuat FaceLift=FQuat::FindBetweenNormals(Forward,(Forward*.72f+Up*.69f).GetSafeNormal());
            for(const TCHAR* Name:{TEXT("neck_01"),TEXT("head")})
            {
                const auto Bone=Index(Name);
                if(!Bone.IsValid())continue;
                const FTransform Base=Output.Pose[Bone];
                FTransform Raised=Component(Bone);
                const float Share=FCString::Strcmp(Name,TEXT("neck_01"))==0?.4f:.6f;
                Raised.SetRotation((FQuat::Slerp(FQuat::Identity,FaceLift,Share)*Raised.GetRotation()).GetNormalized());
                FTransform Blend;
                Blend.Blend(Base,Raised.GetRelativeTransform(Component(Bones.GetParentBoneIndex(Bone))),CrawlBlend);
                Output.Pose[Bone]=Blend;
            }
            Output.Pose.NormalizeRotations();
            return true;
        }
        if (PigBlend>.001f)
        {
            const auto U=Index(TEXT("upperarm_r")), L=Index(TEXT("lowerarm_r")), H=Index(TEXT("hand_r"));
            if(U.IsValid() && L.IsValid() && H.IsValid())
            {
                const FTransform BaseUpper=Output.Pose[U], BaseLower=Output.Pose[L], BaseHand=Output.Pose[H];
                FTransform Upper=Component(U), Lower=Component(L), Hand=Component(H);
                // The finger direction remains across the held leg while its
                // proximal axis turns over during the lift. This gives a
                // continuous palm roll, including the upside-down hold.
                const FQuat Palm=FRotationMatrix::MakeFromXY(PigForward.GetSafeNormal(),PigAxis.GetSafeNormal()).ToQuat();
                const FQuat HandRotation=(Palm*FSeniorDouliIdle::HandFrame(Bones.GetReferenceSkeleton(),true).Inverse()).GetNormalized();
                const FVector Wrist=PigTarget-HandRotation.RotateVector(FSeniorPigHoldPose::GripOffset(Bones.GetReferenceSkeleton()));
                const FVector Pole=Upper.GetLocation()+FVector(FirstPerson?-12.f:-30.f,FirstPerson?42.f:-12.f,-24.f);
                // No stretch: a too-distant floor target remains a reach, not
                // a rubber arm. The physical lift brings the leg into range.
                AnimationCore::SolveTwoBoneIK(Upper,Lower,Hand,Pole,Wrist,false,1.,1.);
                Hand.SetRotation(HandRotation);
                FTransform Blended;
                Blended.Blend(BaseUpper,Upper.GetRelativeTransform(Component(Bones.GetParentBoneIndex(U))),PigBlend); Output.Pose[U]=Blended;
                Blended.Blend(BaseLower,Lower.GetRelativeTransform(Upper),PigBlend); Output.Pose[L]=Blended;
                Blended.Blend(BaseHand,Hand.GetRelativeTransform(Lower),PigBlend); Output.Pose[H]=Blended;
                const FVector Finger=Hand.TransformVectorNoScale(FSeniorDouliIdle::HandFrame(Bones.GetReferenceSkeleton(),true).GetAxisX());
                const FVector PalmNormal=Hand.TransformVectorNoScale(FSeniorDouliIdle::HandFrame(Bones.GetReferenceSkeleton(),true).GetAxisZ());
                const FVector BendAxis=FVector::CrossProduct(Finger,-PalmNormal).GetSafeNormal();
                for (const TCHAR* Name:{TEXT("index"),TEXT("middle"),TEXT("ring"),TEXT("pinky")})
                    for(int32 Joint=1;Joint<=3;++Joint)
                    {
                        const auto J=Index(*FString::Printf(TEXT("%s_0%d_r"),Name,Joint));
                        if(!J.IsValid())continue;
                        const FVector Axis=Component(Bones.GetParentBoneIndex(J)).GetRotation().Inverse().RotateVector(BendAxis);
                        const float Angle=Joint==1?47.f:(Joint==2?68.f:42.f);
                        Output.Pose[J].SetRotation((FQuat(Axis,FMath::DegreesToRadians(Angle*PigGrip))*Output.Pose[J].GetRotation()).GetNormalized());
                    }
                // The thumb opposes the fingers around the rear ankle instead
                // of remaining flat on top of a closed fist.
                for(int32 Joint=1;Joint<=3;++Joint)
                {
                    const auto J=Index(*FString::Printf(TEXT("thumb_0%d_r"),Joint));
                    if(!J.IsValid())continue;
                    const FVector Axis=Component(Bones.GetParentBoneIndex(J)).GetRotation().Inverse().RotateVector(-BendAxis);
                    const float Angle=Joint==1?24.f:(Joint==2?35.f:20.f);
                    Output.Pose[J].SetRotation((FQuat(Axis,FMath::DegreesToRadians(Angle*PigGrip))*Output.Pose[J].GetRotation()).GetNormalized());
                }
            }
            if(FirstPerson)
            {
                // Keep the unoccupied hand relaxed, below the carried pig.
                // It does not pretend to support the animal a second time.
                const auto U0=Index(TEXT("upperarm_l")), L0=Index(TEXT("lowerarm_l")), H0=Index(TEXT("hand_l"));
                if(U0.IsValid() && L0.IsValid() && H0.IsValid())
                {
                    const FTransform A0=Output.Pose[U0], B0=Output.Pose[L0], C0=Output.Pose[H0];
                    FTransform A=Component(U0), B=Component(L0), C=Component(H0);
                    const FVector Target=A.GetLocation()+FVector(14.f,-13.f,-43.f);
                    AnimationCore::SolveTwoBoneIK(A,B,C,A.GetLocation()+FVector(-12.f,-35.f,-18.f),Target,false,1.,1.);
                    FTransform Blend;
                    Blend.Blend(A0,A.GetRelativeTransform(Component(Bones.GetParentBoneIndex(U0))),PigBlend);Output.Pose[U0]=Blend;
                    Blend.Blend(B0,B.GetRelativeTransform(A),PigBlend);Output.Pose[L0]=Blend;
                    Blend.Blend(C0,C.GetRelativeTransform(B),PigBlend);Output.Pose[H0]=Blend;
                }
            }
            Output.Pose.NormalizeRotations();
            return true;
        }
        if(FurnitureBlend>.001f)
        {
            // Medium pieces: lean back slightly to counterbalance the load.
            const float CarryLeanBack=FurnitureSize==ESSOCarrySize::Medium?5.f*FurnitureBlend:0.f;
            if(!FirstPerson && (FurnitureCrouch>.01f || FurnitureLean>.01f || FMath::Abs(FurnitureTwist)>.05f || CarryLeanBack>.05f))
            {
                const auto Pelvis=Index(TEXT("pelvis"));
                FVector Feet[2];
                for(int32 Side=0;Side<2;++Side)
                { const auto Foot=Index(Side==0?TEXT("foot_r"):TEXT("foot_l"));Feet[Side]=Foot.IsValid()?Component(Foot).GetLocation():FVector::ZeroVector; }
                if(Pelvis.IsValid())
                {
                    FTransform T=Component(Pelvis);T.AddToTranslation(FVector(0,0,-FurnitureCrouch));
                    Output.Pose[Pelvis]=T.GetRelativeTransform(Component(Bones.GetParentBoneIndex(Pelvis)));
                }
                // Observers see a real squat and waist bend for a low grip.
                // The feet remain planted while the hands reach the object.
                for(const TCHAR* Name:{TEXT("spine_01"),TEXT("spine_03")})
                {
                    const auto Spine=Index(Name);if(!Spine.IsValid())continue;
                    FTransform T=Component(Spine);
                    const float Degrees=FCString::Strcmp(Name,TEXT("spine_01"))==0?35.f:20.f;
                    const float LeanDegrees=FCString::Strcmp(Name,TEXT("spine_01"))==0?14.f:9.f;
                    const bool bLower=FCString::Strcmp(Name,TEXT("spine_01"))==0;
                    const float Twist=FurnitureTwist*(bLower?.4f:.6f);
                    const float Back=CarryLeanBack*(bLower?.4f:.6f);
                    T.SetRotation((FQuat(FVector(0,0,1),FMath::DegreesToRadians(Twist))*FQuat(FVector(1,0,0),FMath::DegreesToRadians(
                        -Degrees*FurnitureCrouch/75.f-LeanDegrees*FurnitureLean/24.f+Back))*T.GetRotation()).GetNormalized());
                    Output.Pose[Spine]=T.GetRelativeTransform(Component(Bones.GetParentBoneIndex(Spine)));
                }
                for(int32 Side=0;Side<2;++Side)
                {
                    const auto U=Index(Side==0?TEXT("thigh_r"):TEXT("thigh_l"));
                    const auto L=Index(Side==0?TEXT("calf_r"):TEXT("calf_l"));
                    const auto F=Index(Side==0?TEXT("foot_r"):TEXT("foot_l"));
                    if(!U.IsValid() || !L.IsValid() || !F.IsValid())continue;
                    FTransform A=Component(U),B=Component(L),C=Component(F);
                    AnimationCore::SolveTwoBoneIK(A,B,C,B.GetLocation()+FVector(0,45,12),Feet[Side],false,1.,1.);
                    Output.Pose[U]=A.GetRelativeTransform(Component(Bones.GetParentBoneIndex(U)));
                    Output.Pose[L]=B.GetRelativeTransform(A);Output.Pose[F]=C.GetRelativeTransform(B);
                }
            }
            // Solve both hands onto the selected triangles every evaluated
            // frame. The furniture stays a physical body; IK follows it.
            for(int32 Side=0;Side<2;++Side)
            {
                const bool Right=Side==0;
                const auto U=Index(Right?TEXT("upperarm_r"):TEXT("upperarm_l"));
                const auto L=Index(Right?TEXT("lowerarm_r"):TEXT("lowerarm_l"));
                const auto H=Index(Right?TEXT("hand_r"):TEXT("hand_l"));
                if(!U.IsValid() || !L.IsValid() || !H.IsValid())continue;
                const FTransform A0=Output.Pose[U],B0=Output.Pose[L],C0=Output.Pose[H];
                FTransform Upper=Component(U),Lower=Component(L),Hand=Component(H);
                if(!Right && !TwoFurnitureHands)
                {
                    // The free hand hangs relaxed, and reaches out for balance
                    // during a one-handed wind-up.
                    const FVector Balance=(FirstPerson?FVector(26,-6,30):FVector(6,26,30))*FMath::Max(FurnitureWindup,.6f*FurnitureFollow);
                    const FVector Relaxed=Upper.GetLocation()+(FirstPerson?FVector(14,-13,-43):FVector(12,12,-43))+Balance;
                    AnimationCore::SolveTwoBoneIK(Upper,Lower,Hand,Upper.GetLocation()+FVector(-12,-35,-18),Relaxed,false,1.,1.);
                    FTransform Blend;
                    Blend.Blend(A0,Upper.GetRelativeTransform(Component(Bones.GetParentBoneIndex(U))),FurnitureBlend);Output.Pose[U]=Blend;
                    Blend.Blend(B0,Lower.GetRelativeTransform(Upper),FurnitureBlend);Output.Pose[L]=Blend;
                    Blend.Blend(C0,Hand.GetRelativeTransform(Lower),FurnitureBlend);Output.Pose[H]=Blend;
                    continue;
                }
                const FVector PalmBack=FurnitureNormal[Side];
                // The rig frame points out of the back of the hand; keep
                // palms facing the object and the fingernails facing away.
                // Large pieces hook the fingers over the lip; medium pieces
                // hold the sides with the fingers forward; small use the default.
                FVector Fingers=bFurnitureFinger[Side]?FVector::VectorPlaneProject(FurnitureFinger[Side],PalmBack).GetSafeNormal():FVector::ZeroVector;
                if(Fingers.IsNearlyZero())Fingers=FVector::VectorPlaneProject(FurnitureUp,PalmBack).GetSafeNormal();
                if(Fingers.IsNearlyZero())Fingers=FVector::CrossProduct(PalmBack,FVector(0,1,0)).GetSafeNormal();
                const FQuat Palm=FRotationMatrix::MakeFromXZ(Fingers,PalmBack).ToQuat();
                const FQuat Rotation=(Palm*FSeniorDouliIdle::HandFrame(Bones.GetReferenceSkeleton(),Right).Inverse()).GetNormalized();
                const FVector Wrist=FurniturePoint[Side]-Rotation.RotateVector(FSeniorFurnitureHoldPose::GripOffset(Bones.GetReferenceSkeleton(),Right));
                const FVector Pole=Upper.GetLocation()+(FirstPerson
                    ?FVector(-14.f,Right?42.f:-42.f,-25.f):FVector(Right?-42.f:42.f,-18.f,-25.f));
                AnimationCore::SolveTwoBoneIK(Upper,Lower,Hand,Pole,Wrist,false,1.,1.);
                Hand.SetRotation(Rotation);
                FTransform Blend;
                Blend.Blend(A0,Upper.GetRelativeTransform(Component(Bones.GetParentBoneIndex(U))),FurnitureBlend);Output.Pose[U]=Blend;
                Blend.Blend(B0,Lower.GetRelativeTransform(Upper),FurnitureBlend);Output.Pose[L]=Blend;
                Blend.Blend(C0,Hand.GetRelativeTransform(Lower),FurnitureBlend);Output.Pose[H]=Blend;
                const FVector BendAxis=FVector::CrossProduct(Fingers,-PalmBack).GetSafeNormal();
                const float Curl=FurnitureSize==ESSOCarrySize::Large?1.25f:(FurnitureSize==ESSOCarrySize::Small?1.15f:1.f);
                for(const TCHAR* Finger:{TEXT("index"),TEXT("middle"),TEXT("ring"),TEXT("pinky")})
                    for(int32 Joint=1;Joint<=3;++Joint)
                    {
                        const auto J=Index(*FString::Printf(TEXT("%s_0%d_%s"),Finger,Joint,Right?TEXT("r"):TEXT("l")));
                        if(!J.IsValid())continue;
                        const FVector Axis=Component(Bones.GetParentBoneIndex(J)).GetRotation().Inverse().RotateVector(BendAxis);
                        const float Angle=(Joint==1?14.f:(Joint==2?38.f:28.f))*Curl;
                        Output.Pose[J].SetRotation((FQuat(Axis,FMath::DegreesToRadians(Angle*FurnitureGrip))*Output.Pose[J].GetRotation()).GetNormalized());
                    }
                for(int32 Joint=1;Joint<=3;++Joint)
                {
                    const auto J=Index(*FString::Printf(TEXT("thumb_0%d_%s"),Joint,Right?TEXT("r"):TEXT("l")));
                    if(!J.IsValid())continue;
                    const FVector Axis=Component(Bones.GetParentBoneIndex(J)).GetRotation().Inverse().RotateVector(-BendAxis);
                    Output.Pose[J].SetRotation((FQuat(Axis,FMath::DegreesToRadians((Joint==2?22.f:12.f)*FurnitureGrip))*Output.Pose[J].GetRotation()).GetNormalized());
                }
            }
            Output.Pose.NormalizeRotations();
            return true;
        }
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
