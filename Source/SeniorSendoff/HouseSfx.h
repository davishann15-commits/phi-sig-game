#pragma once

#include "CoreMinimal.h"
#include <initializer_list>

class UAudioComponent;
class USceneComponent;
class UWorld;

/** What a prop sounds like when it hits something. */
enum class EHouseSurface : uint8 { Wood, Soft, Leather, Metal, Glass, Plastic, Wicker, Paper, Count };

/** Small synthesised sound bank for the house: furniture knocks, breaks, pick-ups, switches and the singing bass. No assets needed. */
namespace HouseSfx
{
    /** Picks a surface from a prop's name plus the names of its materials (keyword match; wood by default). */
    EHouseSurface SurfaceFor(const FString& Names);
    /** Knock / thud. Strength 0..1 scales the volume; heavier pieces sound bigger. */
    void Impact(UWorld* World, const FVector& At, EHouseSurface Surface, float Strength, float MassKg);
    /** Crack (bCrackOnly) or full break. */
    void Break(UWorld* World, const FVector& At, EHouseSurface Surface, float MassKg, bool bCrackOnly);
    /** Kind 0 pick up, 1 set down, 2 throw. */
    void Handle(UWorld* World, const FVector& At, int32 Kind, EHouseSurface Surface, float MassKg);
    /** Kind 0 light switch, 1 pull station handle, 2 plastic button. */
    void Click(UWorld* World, const FVector& At, int32 Kind);
    /** 0 rubber bounce, 1 rim clank, 2 backboard thump, 3 rope swish. Returns audio spawn success. */
    bool Basketball(UWorld* World, const FVector& At, int32 Kind, float ImpactSpeed);
    /** The bass's tune, attached to the fish. Returns the playing component (stop it to cut the song). */
    UAudioComponent* Song(USceneComponent* AttachTo);
}
