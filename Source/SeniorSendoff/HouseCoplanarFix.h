// Editor tool: finds triangles inside a static mesh that lie exactly on top of another triangle facing the same way
// (stair tread plates on the slab, skirting on the wall...). Those z-fight and shimmer under TSR. The smaller one is lifted off by a hair.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HouseCoplanarFix.generated.h"

class UStaticMesh;

UCLASS()
class UHouseCoplanarFixLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Returns the number of triangles lifted (or that would be lifted when bApply is false); -1 on error. Editor builds only.
    UFUNCTION(BlueprintCallable, Category = "House|Tools")
    static int32 LiftCoplanarOverlaps(UStaticMesh* Mesh, float Offset, bool bApply, FString& Report);
    // Deletes triangles whose centroid (or, with bAnyVertex, any vertex) lies inside Min..Max after scaling the mesh by Scale.
    // Returns the number of triangles removed (or that would be); -1 on error. Editor builds only.
    UFUNCTION(BlueprintCallable, Category = "House|Tools")
    static int32 CutTrianglesInBox(UStaticMesh* Mesh, FVector Min, FVector Max, FVector Scale, bool bAnyVertex, bool bApply, FString& Report);
};
