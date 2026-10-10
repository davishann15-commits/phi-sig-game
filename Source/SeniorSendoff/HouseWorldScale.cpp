#include "HouseWorldScale.h"

#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"

float HouseWorldScale::Factor(const UWorld* World)
{
    if (!World || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return 1.f;
    const AWorldSettings* Settings = World->GetWorldSettings();
    return Settings && Settings->ActorHasTag(TEXT("SSOHouseScale110")) ? 1.1f : 1.f;
}

FVector HouseWorldScale::Point(const UWorld* World, const FVector& AuthoredPoint)
{
    return AuthoredPoint * Factor(World);
}

FVector HouseWorldScale::Unpoint(const UWorld* World, const FVector& WorldPoint)
{
    return WorldPoint / Factor(World);
}

FBox HouseWorldScale::Bounds(const UWorld* World, const FBox& AuthoredBounds)
{
    return FBox(Point(World, AuthoredBounds.Min), Point(World, AuthoredBounds.Max));
}

FTransform HouseWorldScale::Transform(const UWorld* World, const FTransform& AuthoredTransform)
{
    FTransform Result = AuthoredTransform;
    const float Scale = Factor(World);
    Result.SetLocation(AuthoredTransform.GetLocation() * Scale);
    Result.SetScale3D(AuthoredTransform.GetScale3D() * Scale);
    return Result;
}
