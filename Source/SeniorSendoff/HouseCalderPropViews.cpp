#include "HouseCalderLandscape.h"
#if !UE_BUILD_SHIPPING
#include "HouseWoodsDetail.h"
#include "HouseExteriorSiteLayout.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace HouseCalderPropViewsImpl
{
struct FView
{
    FVector Eye = FVector::ZeroVector, Target = FVector::ZeroVector;
    bool Valid = false;
};
TWeakObjectPtr<UWorld> CachedWorld;
TMap<FName, FView> Views;
}
#endif

bool HouseCalderLandscape::WoodsPropMacro(UWorld* World, FName View, FVector& Eye, FVector& Target)
{
#if !UE_BUILD_SHIPPING
    const TCHAR* Batch = View == TEXT("detail_calder_macro_fern") ? TEXT("ExteriorAppalachianFerns")
        : View == TEXT("detail_calder_macro_stump") ? TEXT("ExteriorAppalachianStumps") : nullptr;
    if (!World || !Batch) return false;
    if (HouseCalderPropViewsImpl::CachedWorld.Get() != World)
    {
        HouseCalderPropViewsImpl::CachedWorld = World;
        HouseCalderPropViewsImpl::Views.Reset();
    }
    if (!HouseCalderPropViewsImpl::Views.Contains(View))
    {
        HouseCalderPropViewsImpl::FView Result;
        double Nearest = TNumericLimits<double>::Max();
        int32 ChosenIndex = INDEX_NONE;
        const FVector Seed(-3200, 6500, 0);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(CalderPropCamera), false);
        if (APlayerController* Controller = World->GetFirstPlayerController()) Query.AddIgnoredActor(Controller->GetPawn());
        // Decorative shrubs do not collide. A physics-only sight line can
        // point directly through their opaque leaves; retain the vegetation
        // and reject such cameras using actual per-instance visual bounds.
        TArray<FBox> Shrubs;
        for (TActorIterator<AActor> Actor(World); Actor; ++Actor)
        {
            TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Parts(*Actor);
            for (UHierarchicalInstancedStaticMeshComponent* Part : Parts)
            {
                const FString Name = Part->GetName();
                if (!Part->IsVisible() || !Part->GetStaticMesh()
                    || (!Name.Contains(TEXT("Shrub")) && !Name.Contains(TEXT("Bramb")))) continue;
                const FBox Bounds = Part->GetStaticMesh()->GetBoundingBox();
                for (int32 Index = 0; Index < Part->GetInstanceCount(); ++Index)
                {
                    FTransform Placement;
                    Part->GetInstanceTransform(Index, Placement, true);
                    Shrubs.Add(Bounds.TransformBy(Placement));
                }
            }
        }
        for (TActorIterator<AActor> Actor(World); Actor; ++Actor)
        {
            TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Parts(*Actor);
            for (UHierarchicalInstancedStaticMeshComponent* Part : Parts)
            {
                if (Part->GetName() != Batch || !Part->GetStaticMesh()) continue;
                const FBox Bounds = Part->GetStaticMesh()->GetBoundingBox();
                for (int32 Index = 0; Index < Part->GetInstanceCount(); ++Index)
                {
                    FTransform Placement;
                    Part->GetInstanceTransform(Index, Placement, true);
                    const double Distance = FVector::DistSquared2D(Placement.GetLocation(), Seed);
                    if (Distance >= Nearest) continue;
                    const FVector Aim = Placement.TransformPosition(Bounds.GetCenter());
                    for (const FVector Offset : {FVector(180,-180,0), FVector(-180,-180,0), FVector(180,180,0), FVector(-180,180,0)})
                    {
                        FVector Camera = Placement.GetLocation() + Offset;
                        const float ExpectedFloor = HouseWoods::Height(Camera.X, Camera.Y);
                        FHitResult Floor, Block;
                        if (!World->LineTraceSingleByChannel(Floor, FVector(Camera.X, Camera.Y, ExpectedFloor + 35),
                            FVector(Camera.X, Camera.Y, ExpectedFloor - 85), ECC_Visibility, Query)
                            || Floor.ImpactNormal.Z < .70f || FMath::Abs(Floor.ImpactPoint.Z - ExpectedFloor) > 12.f) continue;
                        Camera.Z = Floor.ImpactPoint.Z + 130.;
                        if (World->LineTraceSingleByChannel(Block, Camera, Aim, ECC_Visibility, Query)) continue;
                        bool Obscured = false;
                        for (const FBox& Shrub : Shrubs)
                        {
                            for (const FVector Margin : {FVector::ZeroVector, FVector(20,0,0), FVector(-20,0,0), FVector(0,0,20)})
                            {
                                const FVector End = Aim + Margin;
                                if (FMath::LineBoxIntersection(Shrub, Camera, End, End - Camera))
                                { Obscured = true; break; }
                            }
                            if (Obscured) break;
                        }
                        if (Obscured) continue;
                        Result.Eye = Camera;
                        Result.Target = Aim;
                        Result.Valid = true;
                        Nearest = Distance;
                        ChosenIndex = Index;
                        break;
                    }
                }
            }
        }
        HouseCalderPropViewsImpl::Views.Add(View, Result);
        if (Result.Valid)
        {
            UE_LOG(LogTemp, Display, TEXT("CALDER_WOODS_PROP_CAMERA view=%s batch=%s index=%d eye=%s target=%s eyeAboveActualFloorCm=130 lineOfSight=1 existingInstance=1 shrubBoundsChecked=%d"),
                *View.ToString(), Batch, ChosenIndex, *Result.Eye.ToString(), *Result.Target.ToString(), Shrubs.Num());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("CALDER_WOODS_PROP_CAMERA_FAILED view=%s noClearExistingInstance=1"), *View.ToString());
        }
    }
    const HouseCalderPropViewsImpl::FView& Result = HouseCalderPropViewsImpl::Views[View];
    if (!Result.Valid) return false;
    Eye = Result.Eye;
    Target = Result.Target;
    return true;
#else
    return false;
#endif
}

bool HouseCalderLandscape::RockDetailView(UWorld* World, FName View, FVector& Eye, FVector& Target)
{
#if !UE_BUILD_SHIPPING
    const bool Rock = View == TEXT("detail_calder_rock_beds");
    if (!World || (!Rock && View != TEXT("detail_calder_wall_stone"))) return false;
    if (HouseCalderPropViewsImpl::CachedWorld.Get() != World)
    {
        HouseCalderPropViewsImpl::CachedWorld = World;
        HouseCalderPropViewsImpl::Views.Reset();
    }
    if (!HouseCalderPropViewsImpl::Views.Contains(View))
    {
        HouseCalderPropViewsImpl::FView Result;
        const FVector2D Seed = Rock ? FVector2D(10200, 28000) : FVector2D(-19000, -6850);
        const float Ground = HouseExteriorSite::GroundHeight(Seed.X, Seed.Y);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(CalderRockCamera), true);
        if (APlayerController* Controller = World->GetFirstPlayerController()) Query.AddIgnoredActor(Controller->GetPawn());
        FHitResult Floor, Face;
        if (World->LineTraceSingleByChannel(Floor, FVector(Seed, Ground + 35.f), FVector(Seed, Ground - 85.f), ECC_Visibility, Query)
            && Floor.ImpactNormal.Z > .70f && FMath::Abs(Floor.ImpactPoint.Z - Ground) < 12.f)
        {
            Result.Eye = FVector(Seed, Floor.ImpactPoint.Z + 130.);
            const FVector Aim = Result.Eye + (Rock ? FVector(1100, 0, -30) : FVector(1100, 0, 0));
            if (World->LineTraceSingleByChannel(Face, Result.Eye, Aim, ECC_Visibility, Query)
                && GetNameSafe(Face.GetComponent()).StartsWith(TEXT("CalderWall_")))
            {
                Result.Target = Face.ImpactPoint + Face.ImpactNormal * .5;
                Result.Valid = true;
                UE_LOG(LogTemp, Display, TEXT("CALDER_ROCK_CAMERA view=%s eyeGroundCm=130.0 surface=%s distanceCm=%.2f eye=(%.3f,%.3f,%.3f) target=(%.3f,%.3f,%.3f)"),
                    *View.ToString(), *GetNameSafe(Face.GetComponent()), Face.Distance,
                    Result.Eye.X, Result.Eye.Y, Result.Eye.Z, Result.Target.X, Result.Target.Y, Result.Target.Z);
            }
        }
        if (!Result.Valid) UE_LOG(LogTemp, Warning, TEXT("CALDER_ROCK_CAMERA view=%s physicalSurfaceCheckFailed=1 retainingSeedView=1 floor=%s face=%s"),
            *View.ToString(), *GetNameSafe(Floor.GetComponent()), *GetNameSafe(Face.GetComponent()));
        HouseCalderPropViewsImpl::Views.Add(View, Result);
    }
    const HouseCalderPropViewsImpl::FView& Result = HouseCalderPropViewsImpl::Views.FindChecked(View);
    if (Result.Valid) { Eye = Result.Eye; Target = Result.Target; return true; }
#endif
    return false;
}
