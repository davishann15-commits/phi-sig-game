#include "SeniorCharacterRoster.h"

#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "UObject/SoftObjectPtr.h"

namespace
{
    struct FCharacterAssets
    {
        TSoftObjectPtr<USkeletalMesh> Body;
        TSoftObjectPtr<USkeletalMesh> Arms;
        TSoftObjectPtr<UAnimSequence> Idle;
        TSoftObjectPtr<UAnimSequence> Walk;
        TSoftObjectPtr<UAnimSequence> ArmsIdle;
        TSoftObjectPtr<UTexture2D> Portrait;

        explicit FCharacterAssets(int32 Index)
        {
            const FString ID = FString::Printf(TEXT("C%02d"), Index + 1);
            const FString Folder = FString(TEXT("/Game/Characters/Cobble/")) + ID + TEXT("/");
            auto Path = [&Folder](const FString& Name)
            {
                return FSoftObjectPath(Folder + Name + TEXT(".") + Name);
            };
            Body = Path(TEXT("SK_") + ID);
            Arms = Path(TEXT("SK_") + ID + TEXT("_Arms"));
            Idle = Path(TEXT("AN_") + ID + TEXT("_Idle"));
            Walk = Path(TEXT("AN_") + ID + TEXT("_Walk"));
            ArmsIdle = Path(TEXT("AN_") + ID + TEXT("_ArmsIdle"));
            const FString PortraitName = TEXT("T_") + ID;
            Portrait = FSoftObjectPath(TEXT("/Game/Characters/Cobble/Portraits/") + PortraitName + TEXT(".") + PortraitName);
        }
    };

    FCharacterAssets* Assets(int32 Index)
    {
        if (!SeniorRoster::IsValidIndex(Index)) return nullptr;
        static TArray<FCharacterAssets> Roster = []()
        {
            TArray<FCharacterAssets> Result;
            Result.Reserve(SeniorRoster::Count);
            for (int32 I = 0; I < SeniorRoster::Count; ++I) Result.Emplace(I);
            return Result;
        }();
        return &Roster[Index];
    }
}

bool SeniorRoster::IsValidIndex(int32 Index) { return Index >= 0 && Index < Count; }
FString SeniorRoster::Label(int32 Index)
{
    if (Index == 0) return TEXT("BRAXTON HUNGATE");
    return IsValidIndex(Index) ? FString::Printf(TEXT("CHARACTER %02d"), Index + 1) : TEXT("CHARACTER");
}
USkeletalMesh* SeniorRoster::Body(int32 Index)
{
    FCharacterAssets* Entry = Assets(Index);
    return Entry ? Entry->Body.LoadSynchronous() : nullptr;
}
USkeletalMesh* SeniorRoster::Arms(int32 Index)
{
    FCharacterAssets* Entry = Assets(Index);
    return Entry ? Entry->Arms.LoadSynchronous() : nullptr;
}
UAnimSequence* SeniorRoster::Idle(int32 Index)
{
    FCharacterAssets* Entry = Assets(Index);
    return Entry ? Entry->Idle.LoadSynchronous() : nullptr;
}
UAnimSequence* SeniorRoster::Walk(int32 Index)
{
    FCharacterAssets* Entry = Assets(Index);
    return Entry ? Entry->Walk.LoadSynchronous() : nullptr;
}
UAnimSequence* SeniorRoster::ArmsIdle(int32 Index)
{
    FCharacterAssets* Entry = Assets(Index);
    return Entry ? Entry->ArmsIdle.LoadSynchronous() : nullptr;
}
UTexture2D* SeniorRoster::Portrait(int32 Index)
{
    FCharacterAssets* Entry = Assets(Index);
    return Entry ? Entry->Portrait.LoadSynchronous() : nullptr;
}
