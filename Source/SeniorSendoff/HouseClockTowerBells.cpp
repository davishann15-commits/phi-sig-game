// The campus clock tower's bells: additive church-bell partials synthesised once at 32 kHz on a worker thread,
// plus the Westminster Quarters (1793, public domain) as timed strikes. No recordings are used.
#include "HouseClockTowerBells.h"
#include "Async/Async.h"
#include "Math/RandomStream.h"
#include <atomic>

namespace HouseClockBellsImpl
{
struct FPartial { float Ratio, Amp, T60, Beat; };

// Partials of a true-harmonic church bell as ratios of the strike note (the prime). Beat > 0 splits a partial
// into a slightly mistuned pair, because no cast bell is perfectly round; the pair gives the slow warble of a ring.
const FPartial Partials[] = {
    { .5f,    .52f,  12.f,  .32f },   // hum
    { 1.f,    .40f,  7.5f,  .55f },   // prime
    { 1.188f, .38f,  5.2f,  .85f },   // tierce: the minor third that makes a bell sound like a bell
    { 1.502f, .13f,  3.4f,  0.f },    // quint
    { 2.f,    .58f,  4.4f,  1.1f },   // nominal
    { 2.508f, .19f,  2.5f,  0.f },    // deciem
    { 2.664f, .11f,  2.1f,  0.f },    // undeciem
    { 3.012f, .15f,  1.8f,  0.f },    // duodeciem
    { 4.03f,  .08f,  1.1f,  0.f },    // upper octave
    { 5.36f,  .05f,  .55f,  0.f },
    { 6.14f,  .035f, .36f,  0.f },
    { 7.42f,  .025f, .24f,  0.f },
};

struct FStore
{
    TArray<int16> Pcm[HouseClockBells::NumBells];
    std::atomic<bool> Ready[HouseClockBells::NumBells];
    std::atomic<bool> Started;
    FStore() : Started(false) { for (std::atomic<bool>& R : Ready) R.store(false); }
};
FStore& Store() { static FStore S; return S; }

void Render(HouseClockBells::EBell Bell, TArray<int16>& Out)
{
    const int32 Rate = HouseClockBells::SampleRate;
    const bool bHour = Bell == HouseClockBells::EBell::Hour;
    const double Strike = HouseClockBells::StrikeHz(Bell);
    const double Size = bHour ? 2.0 : 1.0 + .12 * int32(Bell);           // bigger, lower bells ring longer
    // Long enough that the hum is well down before the closing fade (the hour bell's hum lives for ~35 s).
    const float Seconds = bHour ? 20.f : 10.5f + .5f * int32(Bell);
    TArray<double> Dry; Dry.SetNumZeroed(FMath::CeilToInt(Seconds * Rate));
    FRandomStream Rng(7193 + 131 * int32(Bell));

    // Exponentially decaying sine by complex rotation: one multiply per sample instead of a sin() call.
    auto Ring = [&Dry, Rate](double Freq, double Amp, double T60)
    {
        if (Freq >= Rate * .45) return;
        const double W = 2.0 * PI * Freq / Rate, Decay = FMath::Exp(FMath::Loge(.001) / (T60 * Rate));
        const double Cw = FMath::Cos(W) * Decay, Sw = FMath::Sin(W) * Decay;
        double Re = Amp, Im = 0.0;
        for (int32 I = 0; I < Dry.Num(); ++I)
        {
            Dry[I] += Im;
            const double NextRe = Re * Cw - Im * Sw;
            Im = Re * Sw + Im * Cw; Re = NextRe;
            if ((I & 1023) == 0 && FMath::Abs(Re) + FMath::Abs(Im) < Amp * 1e-4) break;
        }
    };
    for (const FPartial& P : Partials)
    {
        // Each bell gets its own slight inharmonicity; the prime and nominal stay true so the ring is in tune.
        const bool bTuned = P.Ratio == 1.f || P.Ratio == 2.f;
        const double Freq = Strike * P.Ratio * (bTuned ? 1.0 : 1.0 + Rng.FRandRange(-.006f, .006f));
        const double Life = P.T60 * Size * FMath::Clamp(FMath::Pow(440.0 / Freq, .25), .6, 1.6);
        if (P.Beat > 0.f) { Ring(Freq - P.Beat * .5, P.Amp * .62, Life); Ring(Freq + P.Beat * .5, P.Amp * .38, Life * .9); }
        else Ring(Freq, P.Amp, Life);
    }
    // The hammer: a bright, quickly damped clank (band-passed noise and two high modes) over the partials.
    {
        const double High = 1.0 - FMath::Exp(-2.0 * PI * 4200.0 / Rate), Low = 1.0 - FMath::Exp(-2.0 * PI * 700.0 / Rate);
        double YH = 0.0, YL = 0.0;
        const int32 Len = FMath::Min(Dry.Num(), Rate / 6);
        for (int32 I = 0; I < Len; ++I)
        {
            const double Noise = Rng.FRandRange(-1.f, 1.f);
            YH += High * (Noise - YH); YL += Low * (Noise - YL);
            Dry[I] += .30 * FMath::Exp(-I / (Rate * .011)) * (YH - YL);
        }
        Ring(Strike * 8.93, .05, .14); Ring(Strike * 11.27, .03, .09);
    }
    for (int32 I = 0; I < 48 && I < Dry.Num(); ++I) Dry[I] *= I / 48.0;   // 1.5 ms: no click, still a hard strike

    // A touch of campus: early reflections off the hall facades and a late slap back from the stadium bowl,
    // all darkened by distance, plus a faint diffuse tail. Spatial attenuation is applied at play time.
    struct FTap { float Delay, Gain; };
    const FTap Taps[] = { { .061f, .24f }, { .097f, .20f }, { .149f, .16f }, { .233f, .13f }, { .41f, .16f }, { .77f, .085f } };
    TArray<double> Dark; Dark.SetNumUninitialized(Dry.Num());
    {
        const double A = 1.0 - FMath::Exp(-2.0 * PI * 2600.0 / Rate);
        double Y = 0.0;
        for (int32 I = 0; I < Dry.Num(); ++I) { Y += A * (Dry[I] - Y); Dark[I] = Y; }
    }
    TArray<double> Mix = Dry; Mix.AddZeroed(FMath::CeilToInt(.95f * Rate));
    for (const FTap& T : Taps)
    {
        const int32 D = FMath::RoundToInt(T.Delay * Rate);
        for (int32 I = 0; I < Dark.Num(); ++I) Mix[I + D] += Dark[I] * T.Gain;
    }
    for (const double CombSeconds : { .0297, .0371, .0437 })
    {
        const int32 D = FMath::RoundToInt(CombSeconds * Rate);
        TArray<double> Comb; Comb.SetNumZeroed(Mix.Num());
        for (int32 I = 0; I < Mix.Num(); ++I) Comb[I] = (I < Dark.Num() ? Dark[I] : 0.0) + (I >= D ? .7 * Comb[I - D] : 0.0);
        for (int32 I = 0; I < Mix.Num(); ++I) Mix[I] += .035 * Comb[I];
    }
    double Peak = 1e-9;
    for (const double V : Mix) Peak = FMath::Max(Peak, FMath::Abs(V));
    // A long raised-cosine fade into silence, so even the last stroke of the hour dies away naturally.
    const int32 FadeLen = FMath::RoundToInt((bHour ? 3.5f : 2.f) * Rate);
    Out.SetNumUninitialized(Mix.Num());
    for (int32 I = 0; I < Mix.Num(); ++I)
    {
        const double Fade = I > Mix.Num() - FadeLen ? .5 - .5 * FMath::Cos(PI * double(Mix.Num() - I) / FadeLen) : 1.0;
        Out[I] = int16(FMath::Clamp(Mix[I] / Peak * .88 * Fade, -1.0, 1.0) * 32000.0);
    }
}
}

float HouseClockBells::StrikeHz(EBell Bell)
{
    // Westminster's ring sits in E major: G#4, F#4, E4, B3 for the quarters, E3 for the hour.
    switch (Bell)
    {
    case EBell::Q1: return 415.30f;
    case EBell::Q2: return 369.99f;
    case EBell::Q3: return 329.63f;
    case EBell::Q4: return 246.94f;
    default: return 164.81f;
    }
}

const TCHAR* HouseClockBells::Name(EBell Bell)
{
    switch (Bell)
    {
    case EBell::Q1: return TEXT("quarter1_G#4");
    case EBell::Q2: return TEXT("quarter2_F#4");
    case EBell::Q3: return TEXT("quarter3_E4");
    case EBell::Q4: return TEXT("quarter4_B3");
    default: return TEXT("hour_E3");
    }
}

void HouseClockBells::Prewarm()
{
    HouseClockBellsImpl::FStore& S = HouseClockBellsImpl::Store();
    if (S.Started.exchange(true)) return;
    Async(EAsyncExecution::ThreadPool, []
    {
        HouseClockBellsImpl::FStore& Bells = HouseClockBellsImpl::Store();
        const double Start = FPlatformTime::Seconds();
        for (int32 B = 0; B < NumBells; ++B)
        {
            HouseClockBellsImpl::Render(EBell(B), Bells.Pcm[B]);
            Bells.Ready[B].store(true);
        }
        UE_LOG(LogTemp, Display, TEXT("CLOCK_TOWER_BELLS rendered=%d rate=%d ms=%.0f"), NumBells, SampleRate, (FPlatformTime::Seconds() - Start) * 1000.0);
    });
}

const TArray<int16>* HouseClockBells::Pcm(EBell Bell)
{
    HouseClockBellsImpl::FStore& S = HouseClockBellsImpl::Store();
    const int32 B = FMath::Clamp(int32(Bell), 0, NumBells - 1);
    return S.Ready[B].load() ? &S.Pcm[B] : nullptr;
}

float HouseClockBells::Westminster(int32 Quarter, int32 Hour12, int32 Seed, TArray<FStrike>& Out)
{
    // The five changes, bells numbered 1 (highest) to 4. Each change is three crotchets and a dotted minim.
    static const uint8 Changes[5][4] = { { 1, 2, 3, 4 }, { 3, 1, 2, 4 }, { 3, 2, 1, 3 }, { 1, 3, 2, 4 }, { 4, 2, 1, 3 } };
    static const TArray<int32> Phrases[4] = { { 0 }, { 1, 2 }, { 3, 4, 0 }, { 1, 2, 3, 4 } };
    constexpr float Beat = .74f, HourGap = 2.6f, HourStep = 3.5f;
    FRandomStream Rng(Seed);
    Out.Reset();
    float T = LeadIn;
    for (const int32 Change : Phrases[FMath::Clamp(Quarter, 1, 4) - 1])
    {
        for (int32 Note = 0; Note < 4; ++Note)
        {
            FStrike S;
            S.Time = FMath::Max(LeadIn, T + Note * Beat + (Out.Num() ? Rng.FRandRange(-.018f, .018f) : 0.f));
            S.Bell = HouseClockBells::EBell(Changes[Change][Note] - 1);
            S.Velocity = (Note == 3 ? .97f : .86f) + Rng.FRandRange(-.04f, .03f);
            Out.Add(S);
        }
        T += 6.f * Beat;
    }
    if (Quarter >= 4)
    {
        T += HourGap - 2.f * Beat;     // the last change's dotted minim, then a breath before the count
        for (int32 H = 0; H < FMath::Clamp(Hour12, 1, 12); ++H)
        {
            FStrike S;
            S.Time = T + Rng.FRandRange(-.03f, .03f); S.Bell = EBell::Hour; S.Velocity = .95f + Rng.FRandRange(-.03f, .03f); S.bHourCount = true;
            Out.Add(S);
            T += HourStep;
        }
        return T - HourStep + 9.f;
    }
    return T - 2.f * Beat + 6.f;
}
