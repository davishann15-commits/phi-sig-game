// The campus interiors' sounds (master job, step 2e), synthesised: no recordings, no voices. Each is a seamless mono loop
// at SoundRate that AHouseCampusInteriors feeds to a procedural wave.
// 0 a dripping tap: a falling plink and a small splash, twice in 3.2 s.
// 1 a TV crowd: a stadium crowd on the replay, swelling, a cheer, a referee's whistle, the band's tag.
// 2 the postgame radio show: its jingle, then talk made of buzzing formants in syllables and phrases, over a phone-line
//   band (it sounds like talk through a wall; there are no words).
// 3 the room tone inside every building: the HVAC's rumble, the 60-cycle hum, the fluorescent tubes' buzz.
#include "HouseCampusInteriorsRooms.h"

namespace HouseCampusInteriorsImpl
{
struct FSynth
{
    uint32 Seed;
    explicit FSynth(uint32 S) : Seed(S) {}
    float Noise() { Seed = Seed * 1664525u + 1013904223u; return float(Seed >> 8) * (2.f / 16777216.f) - 1.f; }
};
/** A one-pole low-pass at Hz. */
struct FLow
{
    float A = 0.f, Y = 0.f;
    explicit FLow(float Hz) : A(1.f - FMath::Exp(-2.f * PI * Hz / SoundRate)) {}
    float Step(float X) { Y += A * (X - Y); return Y; }
};
/** Blends the last Fade samples into the first ones and drops them, so the loop has no seam. */
void Seamless(TArray<float>& F, int32 Fade)
{
    if (F.Num() < Fade * 2) return;
    const int32 N = F.Num() - Fade;
    for (int32 I = 0; I < Fade; ++I)
    {
        const float T = float(I) / Fade;
        F[I] = F[I] * FMath::Sin(T * HALF_PI) + F[N + I] * FMath::Cos(T * HALF_PI);
    }
    F.SetNum(N);
}

void SoundPcm(int32 Kind, TArray<int16>& Out)
{
    FSynth R(1986u + uint32(Kind) * 7919u);
    TArray<float> F;
    float Peak = .3f;
    switch (Kind)
    {
    case 0:
    {
        F.SetNumZeroed(int32(3.2f * SoundRate));
        for (const float At : { .35f, 1.95f })
        {
            const int32 S0 = int32(At * SoundRate);
            float Phase = 0.f;
            for (int32 I = 0; I < int32(.12f * SoundRate); ++I)
            {
                const float T = float(I) / SoundRate, Hz = 900.f + 2100.f * FMath::Exp(-T * 22.f);
                Phase += 2.f * PI * Hz / SoundRate;
                F[S0 + I] += FMath::Sin(Phase) * FMath::Exp(-T * 38.f);
            }
            FLow L(3500.f);
            for (int32 I = 0; I < int32(.06f * SoundRate); ++I) F[S0 + int32(.015f * SoundRate) + I] += .25f * L.Step(R.Noise()) * FMath::Exp(-float(I) / SoundRate * 70.f);
        }
        Peak = .45f;
        break;
    }
    case 1:
    {
        const int32 N = int32(9.4f * SoundRate);
        F.SetNumZeroed(N);
        FLow Lo1(1600.f), Lo2(1600.f), Hi(260.f), Rumble(180.f);
        for (int32 I = 0; I < N; ++I)
        {
            const float T = float(I) / SoundRate;
            const float X = R.Noise();
            const float Band = Lo2.Step(Lo1.Step(X)) - Hi.Step(Lo2.Y);
            float Env = .55f + .25f * FMath::Sin(2.f * PI * T / 4.7f) + .12f * FMath::Sin(2.f * PI * T * .83f);
            Env += .9f * FMath::Exp(-FMath::Square(T - 3.1f) * 2.5f) + .6f * FMath::Exp(-FMath::Square(T - 7.0f) * 4.f);   // the cheers
            F[I] = Band * Env * 2.2f + .25f * Rumble.Step(R.Noise());
            if (T > 5.f && T < 5.32f) F[I] += .16f * FMath::Sin(2.f * PI * 2950.f * T) * (FMath::Fmod(T, .1f) < .07f ? 1.f : .2f);   // the whistle
            if (T > 7.6f && T < 8.8f)
            {
                // The band's tag: four brassy notes.
                static const float Notes[] = { 392.f, 523.f, 659.f, 784.f };
                const int32 Note = FMath::Clamp(int32((T - 7.6f) / .3f), 0, 3);
                const float Ph = 2.f * PI * Notes[Note] * T;
                F[I] += .07f * (FMath::Sin(Ph) + .5f * FMath::Sin(2.f * Ph) + .3f * FMath::Sin(3.f * Ph)) * FMath::Min(1.f, FMath::Fmod(T - 7.6f, .3f) * 30.f);
            }
        }
        Seamless(F, int32(.4f * SoundRate));
        Peak = .5f;
        break;
    }
    case 2:
    {
        const int32 N = int32(14.f * SoundRate);
        F.SetNumZeroed(N);
        FLow Lo(2800.f), Hi(320.f), Lo2(2800.f);
        float Phase = 0.f, Pitch = 118.f;
        for (int32 I = 0; I < N; ++I)
        {
            const float T = float(I) / SoundRate;
            float X = 0.f;
            if (T < 1.6f)
            {
                // The jingle: a rising arpeggio and a held chord.
                static const float Notes[] = { 262.f, 330.f, 392.f, 523.f };
                const int32 Note = FMath::Min(3, int32(T / .25f));
                const float Hz = Notes[Note];
                const float Env = FMath::Min(1.f, FMath::Fmod(T, .25f) * 40.f) * (T > 1.f ? FMath::Exp(-(T - 1.f) * 4.f) : 1.f);
                X = .5f * Env * (FMath::Sin(2.f * PI * Hz * T) + .4f * FMath::Sin(4.f * PI * Hz * T)) + (T > 1.f ? .2f * Env * FMath::Sin(2.f * PI * 392.f * T) : 0.f);
            }
            else
            {
                // Talk: a buzzing voice-like source (harmonics of a wandering pitch) in syllables of 0.15..0.3 s, phrases
                // of 2..3 s with breaths between, two speakers taking turns.
                const float Phrase = FMath::Fmod(T - 1.6f, 2.6f);
                const bool bSecond = FMath::Fmod((T - 1.6f) / 2.6f, 2.f) >= 1.f;
                Pitch = (bSecond ? 165.f : 112.f) * (1.f + .08f * FMath::Sin(2.f * PI * T * .7f) + .05f * FMath::Sin(2.f * PI * T * 3.1f));
                Phase += 2.f * PI * Pitch / SoundRate;
                float Src = 0.f;
                for (int32 H = 1; H <= 12; ++H) Src += FMath::Sin(Phase * H) / H * (H == 3 || H == 4 || H == 8 ? 1.8f : 1.f);
                const float Syll = FMath::Max(0.f, FMath::Sin(2.f * PI * T * (bSecond ? 4.6f : 4.1f))) * (.6f + .4f * FMath::Sin(2.f * PI * T * 1.3f));
                X = Phrase < 2.1f ? Src * Syll * .35f : 0.f;
            }
            const float Band = Lo2.Step(Lo.Step(X)) - Hi.Step(Lo2.Y);
            F[I] = Band * 1.6f + .02f * R.Noise();   // and a little hiss
        }
        Seamless(F, int32(.3f * SoundRate));
        Peak = .45f;
        break;
    }
    default:
    {
        const int32 N = int32(6.f * SoundRate);
        F.SetNumZeroed(N);
        FLow Brown(70.f), Air(900.f);
        float Brn = 0.f;
        for (int32 I = 0; I < N; ++I)
        {
            const float T = float(I) / SoundRate;
            Brn = FMath::Clamp(Brn + R.Noise() * .02f, -1.f, 1.f);
            const float Hvac = Brown.Step(Brn) * 2.5f + .12f * Air.Step(R.Noise());
            const float Hum = .18f * FMath::Sin(2.f * PI * 60.f * T);
            const float Buzz = (.06f * FMath::Sin(2.f * PI * 120.f * T) + .03f * FMath::Sin(2.f * PI * 240.f * T) + .015f * FMath::Sin(2.f * PI * 360.f * T)) * (1.f + .3f * FMath::Sin(2.f * PI * T * .5f));
            F[I] = Hvac + Hum + Buzz;
        }
        Seamless(F, int32(.5f * SoundRate));
        Peak = .22f;
        break;
    }
    }
    float Max = 1e-4f;
    for (const float V : F) Max = FMath::Max(Max, FMath::Abs(V));
    Out.SetNumUninitialized(F.Num());
    for (int32 I = 0; I < F.Num(); ++I) Out[I] = int16(FMath::Clamp(F[I] / Max * Peak, -1.f, 1.f) * 32767.f);
}
}
