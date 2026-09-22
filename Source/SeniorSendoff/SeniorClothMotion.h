#pragma once
#include "CoreMinimal.h"
#include "Animation/MorphTarget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

// Authored cloth shapes driven by a bounded damped spring, not a collision cloth solver.
// No ticking object or network traffic: the visible preview/pawn owns and updates this state.
struct FSeniorClothMotion
{
    float Position = 0, Velocity = 0, LastYaw = 0, Phase = 0;
    bool bInitialized = false;
    TArray<FName> Left, Right, Breath;

    void Bind(USkeletalMeshComponent* Mesh, float Yaw)
    {
        Position = Velocity = Phase = 0; LastYaw = Yaw; bInitialized = true;
        Left.Reset(); Right.Reset(); Breath.Reset();
        if (!Mesh || !Mesh->GetSkeletalMeshAsset()) return;
        for (const UMorphTarget* Morph : Mesh->GetSkeletalMeshAsset()->GetMorphTargets())
        {
            if (!Morph) continue;
            const FString Name = Morph->GetName();
            if (Name.EndsWith(TEXT("ClothTurnLeft"))) Left.Add(Morph->GetFName());
            if (Name.EndsWith(TEXT("ClothTurnRight"))) Right.Add(Morph->GetFName());
            if (Name.EndsWith(TEXT("ClothBreath"))) Breath.Add(Morph->GetFName());
        }
        Apply(Mesh);
    }
    void Advance(float DeltaSeconds, float Yaw, float Speed = 0)
    {
        // A hidden tab, teleport, or stalled frame must not fling the garment.
        if (!bInitialized || DeltaSeconds > .2f || DeltaSeconds <= 0)
        {
            LastYaw = Yaw; Position = Velocity = 0; bInitialized = true; return;
        }
        const float AngularSpeed = FMath::Clamp(FMath::FindDeltaAngleDegrees(LastYaw, Yaw) / DeltaSeconds, -400.f, 400.f);
        LastYaw = Yaw;
        const float Target = FMath::Clamp(-AngularSpeed / 240.f, -.85f, .85f);
        const int32 Steps = FMath::Max(1, FMath::CeilToInt(DeltaSeconds * 120.f));
        const float H = DeltaSeconds / Steps;
        for (int32 I = 0; I < Steps; ++I)
        {
            Velocity += (78.f * (Target - Position) - 11.f * Velocity) * H;
            Position = FMath::Clamp(Position + Velocity * H, -1.f, 1.f);
        }
        Phase = FMath::Fmod(Phase + DeltaSeconds * (1.7f + FMath::Clamp(Speed / 180.f, 0.f, 2.f)), 2.f * PI);
    }
    void Apply(USkeletalMeshComponent* Mesh) const
    {
        if (!Mesh) return;
        for (FName Name : Left) Mesh->SetMorphTarget(Name, FMath::Max(Position, 0.f));
        for (FName Name : Right) Mesh->SetMorphTarget(Name, FMath::Max(-Position, 0.f));
        for (FName Name : Breath) Mesh->SetMorphTarget(Name, .25f + .22f * FMath::Sin(Phase));
    }
    void Update(USkeletalMeshComponent* Mesh, float DeltaSeconds, float Yaw, float Speed = 0)
    {
        if (Left.IsEmpty() && Right.IsEmpty()) return;
        Advance(DeltaSeconds, Yaw, Speed); Apply(Mesh);
    }
};
