// FHallMesh -> a transient UStaticMesh at runtime (master job, Phase 4), the way HouseWoodsStructure.cpp builds the trees.
// A static mesh's draws are cached by the renderer; a procedural mesh's sections are gathered again every frame on the
// render thread, which is where the campus spent its time.
#include "HouseCampusStaticMesh.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"

namespace HouseCampusStaticMeshImpl
{
/** The mesh as a description: one polygon group a material key (as HouseWoodsStructure.cpp does for the trees). */
void Describe(const FHallMesh& Mesh, FMeshDescription& Description)
{
    FStaticMeshAttributes Attributes(Description);
    Attributes.Register();
    auto Positions = Attributes.GetVertexPositions();
    auto Normals = Attributes.GetVertexInstanceNormals();
    auto Tangents = Attributes.GetVertexInstanceTangents();
    auto Signs = Attributes.GetVertexInstanceBinormalSigns();
    auto UVs = Attributes.GetVertexInstanceUVs();
    UVs.SetNumChannels(1);
    auto Slots = Attributes.GetPolygonGroupMaterialSlotNames();
    for (const FName Key : Mesh.Order)
    {
        const FHallSection* Section = Mesh.Sections.Find(Key);
        if (!Section || Section->I.Num() < 3) continue;
        const FPolygonGroupID Group = Description.CreatePolygonGroup();
        Slots[Group] = Key;
        // One vertex instance a section vertex, shared by its triangles (as the procedural mesh shares them).
        TArray<FVertexInstanceID> Instances;
        Instances.Reserve(Section->V.Num());
        for (int32 Index = 0; Index < Section->V.Num(); ++Index)
        {
            const FVertexID Vertex = Description.CreateVertex();
            Positions[Vertex] = FVector3f(Section->V[Index]);
            const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
            Normals[Instance] = Section->N.IsValidIndex(Index) ? FVector3f(Section->N[Index]) : FVector3f(0.f, 0.f, 1.f);
            Tangents[Instance] = Section->T.IsValidIndex(Index) ? FVector3f(Section->T[Index].TangentX) : FVector3f(1.f, 0.f, 0.f);
            Signs[Instance] = 1.f;
            UVs.Set(Instance, 0, Section->UV.IsValidIndex(Index) ? FVector2f(Section->UV[Index]) : FVector2f::ZeroVector);
            Instances.Add(Instance);
        }
        for (int32 Tri = 0; Tri + 2 < Section->I.Num(); Tri += 3)
        {
            const int32 A = Section->I[Tri], B = Section->I[Tri + 1], C = Section->I[Tri + 2];
            if (!Instances.IsValidIndex(A) || !Instances.IsValidIndex(B) || !Instances.IsValidIndex(C) || A == B || B == C || A == C) continue;
            Description.CreatePolygon(Group, TArray<FVertexInstanceID, TInlineAllocator<3>>{ Instances[A], Instances[B], Instances[C] });
        }
    }
}
}

UStaticMesh* HouseCampusStaticMesh::Build(const FHallMesh& Source, FName Name, UHouseBlackHallSubsystem* Look)
{
    if (!Look || Source.NumTriangles() == 0) return nullptr;
    UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UStaticMesh::StaticClass(), Name), RF_Transient);
    Mesh->NeverStream = true;
    Mesh->bAllowCPUAccess = false;
    for (const FName Key : Source.Order)
        if (const FHallSection* Section = Source.Sections.Find(Key)) if (Section->I.Num() >= 3) Mesh->GetStaticMaterials().Add(FStaticMaterial(Look->Mat(Key), Key
#if WITH_EDITORONLY_DATA
            , Key
#endif
        ));
    FMeshDescription Description;
    HouseCampusStaticMeshImpl::Describe(Source, Description);
    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bBuildSimpleCollision = false;
    Params.bFastBuild = true;
    Params.bAllowCpuAccess = false;
    Params.bMarkPackageDirty = false;
    Params.bCommitMeshDescription = false;
    return Mesh->BuildFromMeshDescriptions({ &Description }, Params) ? Mesh : nullptr;
}

TArray<FHallMesh> HouseCampusStaticMesh::Split(const FHallMesh& Source, int32 MaxTriangles)
{
    TArray<FHallMesh> Out;
    Out.AddDefaulted();
    int32 InPiece = 0;
    for (const FName Key : Source.Order)
    {
        const FHallSection* Section = Source.Sections.Find(Key);
        if (!Section) continue;
        TMap<int32, int32> Remap;   // this section's vertex -> its index in the current piece
        for (int32 Tri = 0; Tri + 2 < Section->I.Num(); Tri += 3)
        {
            if (InPiece >= MaxTriangles) { Out.AddDefaulted(); InPiece = 0; Remap.Reset(); }
            FHallSection& D = Out.Last().Sec(Key);
            for (int32 Corner = 0; Corner < 3; ++Corner)
            {
                const int32 Index = Section->I[Tri + Corner];
                int32* Have = Remap.Find(Index);
                if (!Have)
                {
                    Have = &Remap.Add(Index, D.V.Num());
                    D.V.Add(Section->V[Index]);
                    D.N.Add(Section->N.IsValidIndex(Index) ? Section->N[Index] : FVector::UpVector);
                    D.UV.Add(Section->UV.IsValidIndex(Index) ? Section->UV[Index] : FVector2D::ZeroVector);
                    D.T.Add(Section->T.IsValidIndex(Index) ? Section->T[Index] : FProcMeshTangent());
                }
                D.I.Add(*Have);
            }
            ++InPiece;
        }
    }
    if (Out.Last().NumTriangles() == 0 && Out.Num() > 1) Out.Pop();
    return Out;
}
