#pragma once

#include "CoreMinimal.h"

/** The campus clock tower's ring: five bells synthesised in code (additive partials, no recordings) and the
    Westminster Quarters (1793, public domain) as a list of timed strikes. */
namespace HouseClockBells
{
    /** Quarter bells 1..4 (1 = highest) and the hour bell. */
    enum class EBell : uint8 { Q1, Q2, Q3, Q4, Hour, Count };
    constexpr int32 NumBells = int32(EBell::Count);
    constexpr int32 SampleRate = 32000;

    /** Strike note (the prime partial) in Hz. */
    float StrikeHz(EBell Bell);
    const TCHAR* Name(EBell Bell);
    /** Starts rendering every bell on a worker thread; safe to call repeatedly. */
    void Prewarm();
    /** One strike as 16-bit mono PCM at SampleRate (a baked campus echo included), or nullptr while it still renders. */
    const TArray<int16>* Pcm(EBell Bell);

    /** Silence before a chime's (or toll's) first strike, so the belfry hammer can be seen to lift. */
    constexpr float LeadIn = .4f;

    struct FStrike
    {
        float Time = 0.f;          // seconds from the start of the chime
        EBell Bell = EBell::Hour;
        float Velocity = 1.f;      // 0..1
        bool bHourCount = false;   // one of the hour bell's count strikes
    };
    /** Quarter 1..3 = :15/:30/:45; 4 = the full chime at the hour, then Hour12 strikes of the hour bell.
        Seed adds a few milliseconds of mechanical looseness so no two chimes are identical. Returns the length in
        seconds including the last bell's ring-out. */
    float Westminster(int32 Quarter, int32 Hour12, int32 Seed, TArray<FStrike>& Out);
}
