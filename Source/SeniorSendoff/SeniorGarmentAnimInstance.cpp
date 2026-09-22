#include "SeniorGarmentAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace
{
struct FSeniorGarmentProxy : FAnimInstanceProxy
{
    FAnimNode_CopyPoseFromMesh Pose;
    explicit FSeniorGarmentProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
    {
        Pose.bUseAttachedParent = true;
        Pose.bUseMeshPose = true;
        Pose.bCopyCurves = true;
    }
    virtual void Initialize(UAnimInstance* Instance) override
    {
        if (USkeletalMeshComponent* Garment = Instance->GetSkelMeshComponent())
        {
            TInlineComponentArray<USkeletalMeshComponent*> Meshes(Garment->GetOwner());
            for (USkeletalMeshComponent* Mesh : Meshes)
                if (Mesh->GetFName() == TEXT("Body")) Pose.SourceMeshComponent = Mesh;
        }
        FAnimInstanceProxy::Initialize(Instance);
    }
    virtual FAnimNode_Base* GetCustomRootNode() override { return &Pose; }
    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& Nodes) override { Nodes.Add(&Pose); }
};
}
FAnimInstanceProxy* USeniorGarmentAnimInstance::CreateAnimInstanceProxy() { return new FSeniorGarmentProxy(this); }
void USeniorGarmentAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
