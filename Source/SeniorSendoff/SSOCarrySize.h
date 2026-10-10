#pragma once
#include "CoreMinimal.h"
#include "SSOCarrySize.generated.h"

/** How a piece of furniture is handled. Auto picks from mass and footprint. */
UENUM(BlueprintType)
enum class ESSOCarrySize : uint8
{
    Auto    UMETA(DisplayName="Auto (from mass and size)"),
    Small   UMETA(DisplayName="Small - one hand, full speed, long throw"),
    Medium  UMETA(DisplayName="Medium - two hands, slower, short toss"),
    Large   UMETA(DisplayName="Large - lift one side and drag")
};

/** The replicated stage of a furniture hold, used to drive the procedural animation. */
UENUM(BlueprintType)
enum class ESSOCarryPhase : uint8
{
    None,
    Lifting,
    Holding,
    SettingDown,
    ThrowWindup,
    Thrown
};
