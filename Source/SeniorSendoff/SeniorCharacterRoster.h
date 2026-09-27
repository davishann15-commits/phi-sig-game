#pragma once

#include "CoreMinimal.h"

class UAnimSequence;
class USkeletalMesh;
class UTexture2D;

namespace SeniorRoster
{
    inline constexpr int32 Count = 8;

    SENIORSENDOFF_API bool IsValidIndex(int32 Index);
    SENIORSENDOFF_API FString Label(int32 Index);
    SENIORSENDOFF_API USkeletalMesh* Body(int32 Index);
    SENIORSENDOFF_API USkeletalMesh* Arms(int32 Index);
    SENIORSENDOFF_API UAnimSequence* Idle(int32 Index);
    SENIORSENDOFF_API UAnimSequence* Walk(int32 Index);
    SENIORSENDOFF_API UAnimSequence* ArmsIdle(int32 Index);
    SENIORSENDOFF_API UTexture2D* Portrait(int32 Index);
    SENIORSENDOFF_API void AppendSelectionPreviewPaths(TArray<FSoftObjectPath>& OutPaths);
}
