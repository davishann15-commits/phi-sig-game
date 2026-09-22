#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SeniorCharacterAssetTools.generated.h"
class USkeletalMesh;
class UGroomAsset;

UCLASS()
class USeniorCharacterAssetTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Senior Sendoff|Asset Build")
    static bool ExportBraxtonScalpReference();
    // Editor-only implementation used by the character's reproducible import script.
    UFUNCTION(BlueprintCallable, Category="Senior Sendoff|Asset Build")
    static bool PrepareBraxtonDetailLevels(USkeletalMesh* Mesh);
    UFUNCTION(BlueprintCallable, Category="Senior Sendoff|Asset Build")
    static bool BuildBraxtonCloth(USkeletalMesh* Outfit, USkeletalMesh* Body);
    UFUNCTION(BlueprintCallable, Category="Senior Sendoff|Asset Build")
    static bool PrepareBraxtonHoodieExport(USkeletalMesh* Mesh);
    UFUNCTION(BlueprintCallable, Category="Senior Sendoff|Asset Build")
    static bool ShapeBraxtonHair(UGroomAsset* Groom);
    UFUNCTION(BlueprintCallable, Category="Senior Sendoff|Asset Build")
    static bool MatchBraxtonGarmentBindPose(USkeletalMesh* Outfit, USkeletalMesh* Body);
};
