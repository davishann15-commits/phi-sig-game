// Calder State exterior polish, step 3d: the outdoor soundscape's sounds, all synthesised (no recordings, no voices you
// could make out). Loops are seamless; one-shots start and end in silence.
// 0 the crowd going home: a murmur of many talkers (buzzing formants in syllables) with a laugh or a shout now and then;
// 1 wind: gusting brown noise with a faint whistle through the trees; 2 crickets: two of them, chirping out of step;
// 3 a bus idling: the diesel's firing pulses, its rattle; 4 a car horn honking for the Geese (two long, three short);
// 5 a car door shut somewhere in the lot.
#include "HouseCampusPolish.h"

namespace HouseCampusPolishSoundImpl
{
struct FNoise
{
    uint32 Seed;
    explicit FNoise(uint32 S) : Seed(S) {}
    float Next() { Seed = Seed * 1664525u + 1013904223u; return float(Seed >> 8) * (2.f / 16777216.f) - 1.f; }
};
struct FOnePole
{
    float A = 0.f, Y = 0.f;
    explicit FOnePole(float Hz) : A(1.f - FMath::Exp(-2.f * PI * Hz / HouseCampusPolish::SoundRate)) {}
    float Step(float X) { Y += A * (X - Y); return Y; }
};
void Loop(TArray<float>& F, int32 Fade)
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
}

void HouseCampusPolish::SoundPcm(int32 Kind, TArray<int16>& Out)
{
    namespace SI = HouseCampusPolishSoundImpl;
    constexpr int32 Rate = HouseCampusPolish::SoundRate;
    SI::FNoise R(6100u + uint32(Kind) * 104729u);
    TArray<float> F;
    float Peak = .3f;
    switch (Kind)
    {
    case 0:
    {
        // Twenty talkers at different pitches, each in phrases of syllables with pauses; a laugh and a shout.
        const int32 N = int32(12.f * Rate);
        F.SetNumZeroed(N);
        for (int32 V = 0; V < 20; ++V)
        {
            const float Pitch = 95.f + (V % 7) * 22.f + R.Next() * 8.f, SyllHz = 3.4f + (V % 5) * .35f, PhraseLen = 1.6f + (V % 4) * .5f;
            const float Offset = (R.Next() * .5f + .5f) * 12.f, Gain = .5f + .5f * (R.Next() * .5f + .5f);
            float Phase = 0.f;
            SI::FOnePole Lo(1700.f), Hi(250.f);
            for (int32 I = 0; I < N; ++I)
            {
                const float T = float(I) / Rate, Tp = FMath::Fmod(T + Offset, PhraseLen + .8f);
                if (Tp > PhraseLen) continue;
                Phase += 2.f * PI * Pitch * (1.f + .06f * FMath::Sin(2.f * PI * T * .9f + V)) / Rate;
                float Src = 0.f;
                for (int32 H = 1; H <= 9; ++H) Src += FMath::Sin(Phase * H) / H * ((H == 3 || H == 5) ? 1.6f : 1.f);
                const float Syll = FMath::Max(0.f, FMath::Sin(2.f * PI * T * SyllHz + V * 1.3f));
                const float X = Lo.Step(Src * Syll * Gain);
                F[I] += X - Hi.Step(X);
            }
        }
        // A laugh (a run of short bright syllables) and a distant shout.
        for (int32 I = 0; I < int32(.8f * Rate); ++I)
        {
            const float T = float(I) / Rate;
            F[int32(4.2f * Rate) + I] += .5f * FMath::Sin(2.f * PI * 260.f * T) * FMath::Max(0.f, FMath::Sin(2.f * PI * 7.f * T)) * FMath::Exp(-T * 2.f);
            F[int32(8.5f * Rate) + I] += .35f * FMath::Sin(2.f * PI * (330.f - T * 80.f) * T) * FMath::Exp(-T * 3.f) * (T < .5f ? 1.f : 0.f);
        }
        SI::FOnePole Air(3000.f);
        for (int32 I = 0; I < N; ++I) F[I] = Air.Step(F[I]) + .02f * R.Next();
        SI::Loop(F, int32(.5f * Rate));
        Peak = .3f;
        break;
    }
    case 1:
    {
        const int32 N = int32(14.f * Rate);
        F.SetNumZeroed(N);
        SI::FOnePole Lo(260.f), Lo2(260.f), Whistle(1200.f), WhistleHi(900.f);
        float Brown = 0.f;
        for (int32 I = 0; I < N; ++I)
        {
            const float T = float(I) / Rate;
            Brown = FMath::Clamp(Brown + R.Next() * .03f, -1.f, 1.f);
            const float Gust = .45f + .35f * FMath::Sin(2.f * PI * T / 7.f) + .2f * FMath::Sin(2.f * PI * T / 2.3f + 1.f);
            const float Rush = Lo2.Step(Lo.Step(R.Next())) * 3.f;
            const float W = Whistle.Step(R.Next());
            F[I] = (Brown * .6f + Rush) * Gust + (W - WhistleHi.Step(W)) * .4f * FMath::Max(0.f, Gust - .6f);
        }
        SI::Loop(F, int32(1.f * Rate));
        Peak = .25f;
        break;
    }
    case 2:
    {
        // Two field crickets: chirps of three or four 18 ms pulses at 4.6 and 4.2 kHz, about two a second, out of step.
        const int32 N = int32(6.f * Rate);
        F.SetNumZeroed(N);
        for (int32 C = 0; C < 2; ++C)
        {
            const float Hz = C ? 4200.f : 4600.f, Every = C ? .62f : .47f, Gain = C ? .6f : 1.f;
            for (float At = C * .2f; At < 5.6f; At += Every * (1.f + .08f * R.Next()))
                for (int32 Pulse = 0; Pulse < 3 + (int32(At * 10.f) % 2); ++Pulse)
                {
                    const int32 S0 = int32((At + Pulse * .034f) * Rate);
                    for (int32 I = 0; I < int32(.018f * Rate) && S0 + I < N; ++I)
                    {
                        const float T = float(I) / Rate, Env = FMath::Sin(PI * T / .018f);
                        F[S0 + I] += Gain * Env * FMath::Sin(2.f * PI * Hz * T);
                    }
                }
        }
        SI::Loop(F, int32(.1f * Rate));
        Peak = .18f;
        break;
    }
    case 3:
    {
        // A diesel at idle: firing pulses at about 30 a second, each a damped low thump with its harmonics, a little
        // irregular; the rattle of the body over it.
        const int32 N = int32(4.f * Rate);
        F.SetNumZeroed(N);
        float Next = 0.f;
        int32 Cyl = 0;
        while (Next < 4.f)
        {
            const int32 S0 = int32(Next * Rate);
            const float Strength = .8f + .25f * ((Cyl % 6) == 2 ? -.4f : R.Next() * .2f);
            for (int32 I = 0; I < int32(.05f * Rate) && S0 + I < N; ++I)
            {
                const float T = float(I) / Rate;
                F[S0 + I] += Strength * FMath::Exp(-T * 70.f) * (FMath::Sin(2.f * PI * 60.f * T) + .5f * FMath::Sin(2.f * PI * 120.f * T) + .25f * FMath::Sin(2.f * PI * 240.f * T));
            }
            Next += (1.f / 30.f) * (1.f + .03f * R.Next());
            ++Cyl;
        }
        SI::FOnePole Lo(500.f), Rattle(2500.f), RattleHi(1200.f);
        for (int32 I = 0; I < N; ++I)
        {
            const float Rt = Rattle.Step(R.Next());
            F[I] = Lo.Step(F[I]) + (Rt - RattleHi.Step(Rt)) * .12f * (1.f + FMath::Sin(2.f * PI * 30.f * I / Rate));
        }
        SI::Loop(F, int32(.25f * Rate));
        Peak = .4f;
        break;
    }
    case 4:
    {
        // HONK, HONK, honk-honk-honk: the horn's two reeds (420 and 520 Hz), a little rough.
        const int32 N = int32(2.6f * Rate);
        F.SetNumZeroed(N);
        const TPair<float, float> Notes[] = { { 0.f, .38f }, { .55f, .38f }, { 1.15f, .14f }, { 1.38f, .14f }, { 1.61f, .3f } };
        for (const TPair<float, float>& Nt : Notes)
        {
            const int32 S0 = int32(Nt.Key * Rate);
            for (int32 I = 0; I < int32(Nt.Value * Rate) && S0 + I < N; ++I)
            {
                const float T = float(I) / Rate, Env = FMath::Min(1.f, T * 80.f) * FMath::Min(1.f, (Nt.Value - T) * 60.f);
                const float A = FMath::Sin(2.f * PI * 420.f * T), B = FMath::Sin(2.f * PI * 520.f * T);
                F[S0 + I] += Env * (FMath::Clamp(A * 2.5f, -1.f, 1.f) + FMath::Clamp(B * 2.5f, -1.f, 1.f)) * .5f;
            }
        }
        SI::FOnePole Lo(3500.f);
        for (int32 I = 0; I < N; ++I) F[I] = Lo.Step(F[I]);
        Peak = .5f;
        break;
    }
    default:
    {
        // A car door: the slam (a low thud and a burst of noise), the latch's click.
        const int32 N = int32(.7f * Rate);
        F.SetNumZeroed(N);
        SI::FOnePole Lo(900.f);
        for (int32 I = 0; I < N; ++I)
        {
            const float T = float(I) / Rate;
            F[I] = FMath::Exp(-T * 18.f) * FMath::Sin(2.f * PI * 72.f * T) * .9f + Lo.Step(R.Next()) * FMath::Exp(-T * 40.f) * 1.4f;
            if (T > .045f && T < .052f) F[I] += .5f * R.Next();
        }
        Peak = .55f;
        break;
    }
    }
    float Max = 1e-4f;
    for (const float V : F) Max = FMath::Max(Max, FMath::Abs(V));
    Out.SetNumUninitialized(F.Num());
    for (int32 I = 0; I < F.Num(); ++I) Out[I] = int16(FMath::Clamp(F[I] / Max * Peak, -1.f, 1.f) * 32767.f);
}
