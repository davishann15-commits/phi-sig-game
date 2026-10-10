// Synthesised one-shot sounds for furniture and gadgets (16 kHz mono PCM, built once and cached).
#include "HouseSfx.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RandomStream.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "TimerManager.h"
#include "UObject/Package.h"

namespace
{
constexpr int32 SfxRate = 16000;

struct FSfxBuf
{
    TArray<float> S;
    explicit FSfxBuf(float Seconds) { S.SetNumZeroed(FMath::CeilToInt(Seconds * SfxRate)); }
    /** Decaying sine: Amp * exp(-Decay t) * sin(2 pi f t), starting at Start seconds. */
    void Sine(float Freq, float Amp, float Decay, float Start = 0.f)
    {
        const int32 I0 = FMath::Clamp(FMath::RoundToInt(Start * SfxRate), 0, S.Num());
        for (int32 I = I0; I < S.Num(); ++I)
        {
            const float T = (I - I0) / float(SfxRate);
            const float Env = FMath::Exp(-Decay * T);
            if (Env < .002f) break;
            S[I] += Amp * Env * FMath::Sin(2.f * PI * Freq * T);
        }
    }
    /** Low-passed white noise burst with exponential decay (Attack seconds of fade-in). */
    void Noise(FRandomStream& R, float Amp, float Decay, float LowpassHz, float Start = 0.f, float Attack = 0.f)
    {
        const int32 I0 = FMath::Clamp(FMath::RoundToInt(Start * SfxRate), 0, S.Num());
        const float A = FMath::Clamp(2.f * PI * LowpassHz / SfxRate, 0.f, 1.f);
        float Y = 0.f;
        for (int32 I = I0; I < S.Num(); ++I)
        {
            const float T = (I - I0) / float(SfxRate);
            const float Env = FMath::Exp(-Decay * T) * (Attack > 0.f ? FMath::Min(1.f, T / Attack) : 1.f);
            if (T > Attack && Env < .002f) break;
            Y += A * (R.FRandRange(-1.f, 1.f) - Y);
            S[I] += Amp * Env * Y * 2.2f;
        }
    }
    TArray<int16> Finish(float Gain) const
    {
        TArray<int16> Out; Out.SetNumUninitialized(S.Num());
        for (int32 I = 0; I < S.Num(); ++I)
        {
            const float X = S[I] * Gain;
            const float Soft = X / (1.f + FMath::Abs(X) * .35f);   // gentle soft clip
            const float Fade = I > S.Num() - 160 ? (S.Num() - I) / 160.f : 1.f;
            Out[I] = int16(FMath::Clamp(Soft * Fade, -1.f, 1.f) * 32000.f);
        }
        return Out;
    }
};

/** One knock of a surface into Buf at Start with amplitude Amp (0..1). */
void SfxKnock(FSfxBuf& B, FRandomStream& R, EHouseSurface Surface, float Amp, float Start, float Size)
{
    const float Big = FMath::Clamp(Size, 0.f, 1.f);        // 0 small object, 1 heavy furniture
    const float Lo = FMath::Lerp(1.25f, .7f, Big);         // heavy things ring lower
    switch (Surface)
    {
    case EHouseSurface::Soft:
        B.Noise(R, Amp * .55f, 55.f, 520.f, Start);
        B.Sine(R.FRandRange(62.f, 80.f), Amp * .35f, 26.f, Start);
        break;
    case EHouseSurface::Leather:
        B.Noise(R, Amp * .5f, 48.f, 900.f, Start);
        B.Sine(R.FRandRange(95.f, 125.f) * Lo, Amp * .5f, 28.f, Start);
        B.Sine(R.FRandRange(260.f, 330.f), Amp * .2f, 70.f, Start);
        break;
    case EHouseSurface::Metal:
    {
        B.Noise(R, Amp * .45f, 300.f, 6000.f, Start);
        const float F = R.FRandRange(760.f, 900.f) * Lo;
        B.Sine(F, Amp * .5f, 7.f, Start); B.Sine(F * 1.6f, Amp * .35f, 9.f, Start);
        B.Sine(F * 2.51f, Amp * .25f, 12.f, Start); B.Sine(F * 3.53f, Amp * .15f, 16.f, Start);
        break;
    }
    case EHouseSurface::Glass:
    {
        B.Noise(R, Amp * .4f, 250.f, 7000.f, Start);
        const float F = R.FRandRange(2200.f, 2600.f);
        B.Sine(F, Amp * .4f, 12.f, Start); B.Sine(F * 1.62f, Amp * .3f, 16.f, Start); B.Sine(F * 2.33f, Amp * .2f, 20.f, Start);
        break;
    }
    case EHouseSurface::Plastic:
        B.Noise(R, Amp * .4f, 250.f, 2500.f, Start);
        B.Sine(R.FRandRange(220.f, 270.f) * Lo, Amp * .5f, 35.f, Start);
        B.Sine(R.FRandRange(850.f, 1000.f), Amp * .3f, 90.f, Start);
        break;
    case EHouseSurface::Wicker:
        B.Noise(R, Amp * .5f, 40.f, 2000.f, Start);
        for (int32 K = 0; K < 5; ++K) B.Noise(R, Amp * .3f, 400.f, 3500.f, Start + R.FRandRange(.005f, .09f));
        break;
    case EHouseSurface::Paper:
        B.Noise(R, Amp * .5f, 45.f, 1500.f, Start);
        B.Sine(R.FRandRange(130.f, 170.f), Amp * .2f, 40.f, Start);
        break;
    default: // wood
        B.Noise(R, Amp * .6f, 400.f, 3000.f, Start);
        B.Sine(R.FRandRange(160.f, 220.f) * Lo, Amp * .55f, 30.f, Start);
        B.Sine(R.FRandRange(620.f, 820.f) * Lo, Amp * .35f, 70.f, Start);
        B.Sine(R.FRandRange(1300.f, 1700.f), Amp * .2f, 110.f, Start);
        break;
    }
}

enum class ESfxKind : uint8 { Impact, Crack, Break, Pickup, SetDown, Throw, Switch, Pull, Button, Song };

uint32 SfxKey(ESfxKind Kind, EHouseSurface Surface, int32 Variant, int32 SizeBucket)
{
    return (uint32(Kind) << 24) | (uint32(Surface) << 16) | (uint32(SizeBucket) << 8) | uint32(Variant);
}

/** Builds (and caches) one sound. SizeBucket 0..3 from mass. */
const TArray<int16>& SfxPcm(ESfxKind Kind, EHouseSurface Surface, int32 Variant, int32 SizeBucket)
{
    static TMap<uint32, TArray<int16>> Cache;
    const uint32 Key = SfxKey(Kind, Surface, Variant, SizeBucket);
    if (const TArray<int16>* Found = Cache.Find(Key)) return *Found;
    FRandomStream R(int32(Key * 2654435761u));
    const float Size = SizeBucket / 3.f;
    float Gain = 1.f;
    FSfxBuf B(.1f);
    switch (Kind)
    {
    case ESfxKind::Impact:
        B = FSfxBuf(Surface == EHouseSurface::Metal ? .8f : .4f);
        SfxKnock(B, R, Surface, 1.f, 0.f, Size);
        if (Size > .5f) SfxKnock(B, R, Surface, .35f, R.FRandRange(.03f, .07f), Size);   // heavy pieces double-hit
        break;
    case ESfxKind::Crack:
        B = FSfxBuf(.5f);
        B.Noise(R, .9f, 60.f, 4000.f);
        for (int32 K = 0; K < 3; ++K) SfxKnock(B, R, Surface, .5f, R.FRandRange(.01f, .16f), Size);
        break;
    case ESfxKind::Break:
    {
        B = FSfxBuf(Surface == EHouseSurface::Glass ? .9f : .75f);
        B.Noise(R, 1.f, 28.f, Surface == EHouseSurface::Glass ? 7000.f : 3500.f);
        const int32 Knocks = Surface == EHouseSurface::Glass ? 12 : 6 + SizeBucket * 2;
        for (int32 K = 0; K < Knocks; ++K)
            SfxKnock(B, R, Surface, FMath::Lerp(.8f, .25f, K / float(Knocks)), R.FRandRange(.0f, .42f), Size * .7f);
        Gain = 1.2f;
        break;
    }
    case ESfxKind::Pickup:
        B = FSfxBuf(.3f);
        B.Noise(R, .3f, 40.f, 1300.f, 0.f, .02f);
        SfxKnock(B, R, Surface, .22f, .04f, Size);
        Gain = .7f;
        break;
    case ESfxKind::SetDown:
        B = FSfxBuf(.4f);
        SfxKnock(B, R, Surface, .45f, 0.f, Size);
        B.Noise(R, .15f, 30.f, 900.f, .02f);
        Gain = .8f;
        break;
    case ESfxKind::Throw:
        B = FSfxBuf(.35f);
        B.Noise(R, .5f, 16.f, 1800.f, 0.f, .09f);
        Gain = .6f;
        break;
    case ESfxKind::Switch:
        B = FSfxBuf(.12f);
        B.Noise(R, .6f, 500.f, 5000.f); B.Sine(2600.f, .3f, 220.f);
        B.Noise(R, .4f, 500.f, 4000.f, .035f); B.Sine(1900.f, .25f, 220.f, .035f);
        Gain = .9f;
        break;
    case ESfxKind::Pull:
        B = FSfxBuf(.4f);
        B.Noise(R, .5f, 120.f, 1500.f); B.Sine(180.f, .5f, 40.f); B.Sine(1200.f, .25f, 60.f);
        B.Noise(R, .5f, 300.f, 3000.f, .09f); B.Sine(900.f, .3f, 50.f, .09f);
        break;
    case ESfxKind::Button:
        B = FSfxBuf(.12f);
        B.Noise(R, .5f, 400.f, 2500.f); B.Sine(900.f, .3f, 120.f);
        Gain = .8f;
        break;
    case ESfxKind::Song:
    {
        // A goofy little tune of our own: square-ish voice with vibrato, a thump on each beat.
        const float Beat = .3f;
        const int32 Notes[] = { 64, 67, 69, 67, 64, 62, 64, -1, 64, 67, 69, 72, 69, 67, 64, -1 };
        const int32 N = UE_ARRAY_COUNT(Notes);
        B = FSfxBuf(Beat * N + .3f);
        for (int32 K = 0; K < N; ++K)
        {
            const float T0 = K * Beat;
            if (K % 2 == 0) { B.Sine(70.f, .35f, 22.f, T0); B.Noise(R, .2f, 90.f, 400.f, T0); }
            if (Notes[K] < 0) continue;
            const float F = 440.f * FMath::Pow(2.f, (Notes[K] - 69) / 12.f);
            const int32 I0 = FMath::RoundToInt(T0 * SfxRate), I1 = FMath::Min(B.S.Num(), FMath::RoundToInt((T0 + Beat * .9f) * SfxRate));
            float Phase = 0.f;
            for (int32 I = I0; I < I1; ++I)
            {
                const float T = (I - I0) / float(SfxRate);
                const float Env = FMath::Min(1.f, T / .015f) * FMath::Min(1.f, (I1 - I) / float(SfxRate) / .04f);
                const float Vib = 1.f + .012f * FMath::Sin(2.f * PI * 6.5f * T);
                Phase += 2.f * PI * F * Vib / SfxRate;
                const float Sq = FMath::Sin(Phase) + FMath::Sin(3.f * Phase) / 3.f + FMath::Sin(5.f * Phase) / 5.f;
                B.S[I] += .32f * Env * Sq;
            }
        }
        Gain = .9f;
        break;
    }
    }
    return Cache.Add(Key, B.Finish(Gain));
}

USoundAttenuation* SfxAttenuation(float Inner, float Falloff)
{
    USoundAttenuation* Att = NewObject<USoundAttenuation>(GetTransientPackage());
    Att->Attenuation.bAttenuate = true;
    Att->Attenuation.bSpatialize = true;
    Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    Att->Attenuation.AttenuationShapeExtents = FVector(Inner, 0.f, 0.f);
    Att->Attenuation.FalloffDistance = Falloff;
    return Att;
}

USoundWaveProcedural* SfxWave(const TArray<int16>& Pcm)
{
    USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
    Wave->SetSampleRate(SfxRate);
    Wave->NumChannels = 1;
    Wave->Duration = Pcm.Num() / float(SfxRate);
    Wave->SoundGroup = SOUNDGROUP_Default;
    Wave->bLooping = false;
    Wave->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
    return Wave;
}

/** Plays one cached clip at a location. Cheap global rate limit so a pile-up never floods the mixer. */
void SfxPlay(UWorld* World, const TArray<int16>& Pcm, const FVector& At, float Volume, float Pitch, float Inner, float Falloff)
{
    if (!World || World->GetNetMode() == NM_DedicatedServer || Pcm.Num() == 0 || Volume <= .01f) return;
    static double Window = 0.0; static int32 InWindow = 0;
    const double Now = FPlatformTime::Seconds();
    if (Now - Window > .25) { Window = Now; InWindow = 0; }
    if (++InWindow > 6) return;
    UAudioComponent* Comp = UGameplayStatics::SpawnSoundAtLocation(World, SfxWave(Pcm), At, FRotator::ZeroRotator,
        FMath::Clamp(Volume, 0.f, 1.5f), Pitch, 0.f, SfxAttenuation(Inner, Falloff), nullptr, true);
    if (Comp)
    {
        // Procedural waves pad with silence on underrun; cut the component when the clip is over.
        const float Length = Pcm.Num() / float(SfxRate) / FMath::Max(.5f, Pitch) + .05f;
        FTimerHandle Handle;
        TWeakObjectPtr<UAudioComponent> Weak(Comp);
        World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak] { if (UAudioComponent* C = Weak.Get()) { C->Stop(); C->DestroyComponent(); } }), Length, false);
    }
}

int32 SfxSizeBucket(float MassKg) { return MassKg < 3.f ? 0 : (MassKg < 12.f ? 1 : (MassKg < 40.f ? 2 : 3)); }
int32 SfxVariant() { return FMath::RandRange(0, 2); }
float SfxPitch() { return FMath::FRandRange(.93f, 1.07f); }
}

EHouseSurface HouseSfx::SurfaceFor(const FString& NamesIn)
{
    const FString Names = NamesIn.ToLower();
    auto Has = [&Names](std::initializer_list<const TCHAR*> Words) { for (const TCHAR* W : Words) if (Names.Contains(W)) return true; return false; };
    if (Has({ TEXT("glass"), TEXT("jar"), TEXT("bottle"), TEXT("mirror"), TEXT("window"), TEXT("vase"), TEXT("ceramic"), TEXT("bowl"), TEXT("plate"),
        TEXT("mug"), TEXT("toilet"), TEXT("sink"), TEXT("dispenser"), TEXT("framed"), TEXT("photo") })) return EHouseSurface::Glass;
    if (Has({ TEXT("wicker"), TEXT("basket"), TEXT("rattan") })) return EHouseSurface::Wicker;
    if (Has({ TEXT("pillow"), TEXT("cushion"), TEXT("blanket"), TEXT("towel"), TEXT("cloth"), TEXT("fabric"), TEXT("laundry"), TEXT("bag"), TEXT("shirt"),
        TEXT("mattress"), TEXT("flag"), TEXT("banner"), TEXT("pennant"), TEXT("curtain"), TEXT("rug"), TEXT("door mat"), TEXT("doormat"), TEXT("bean"), TEXT("linen"),
        TEXT("plant"), TEXT("bouquet"), TEXT("flower"), TEXT("hoodie"), TEXT("jersey"), TEXT("sock"), TEXT("beanie"), TEXT("cap") })) return EHouseSurface::Soft;
    if (Has({ TEXT("leather"), TEXT("recliner"), TEXT("couch"), TEXT("sofa"), TEXT("futon"), TEXT("armchair") })) return EHouseSurface::Leather;
    if (Has({ TEXT("paper"), TEXT("cardboard"), TEXT("box"), TEXT("book"), TEXT("poster"), TEXT("magazine"), TEXT("pizza"), TEXT("carton") })) return EHouseSurface::Paper;
    if (Has({ TEXT("plastic"), TEXT("jug"), TEXT("wastebin"), TEXT("trash"), TEXT("bucket"), TEXT("tv"), TEXT("television"), TEXT("monitor"),
        TEXT("keyboard"), TEXT("vacuum"), TEXT("vinyl"), TEXT("cups"), TEXT("clearcup"), TEXT("solo cup"), TEXT("crate"), TEXT("fan"), TEXT("printer"), TEXT("console"), TEXT("chip"),
        TEXT("cord"), TEXT("cable"), TEXT("controller"), TEXT("remote"), TEXT("phone"), TEXT("laptop"), TEXT("canister"), TEXT("solo") })) return EHouseSurface::Plastic;
    if (Has({ TEXT("metal"), TEXT("steel"), TEXT("chrome"), TEXT("iron"), TEXT("alumin"), TEXT("fridge"), TEXT("microwave"), TEXT("keg"), TEXT("cans"), TEXT("beer can"), TEXT("soda can"),
        TEXT("lamp"), TEXT("pipe"), TEXT("rack"), TEXT("stool"), TEXT("saucepan"), TEXT("cookpot"), TEXT("tray"), TEXT("grill"), TEXT("locker"), TEXT("ice machine"), TEXT("icemaker"),
        TEXT("stainless"), TEXT("cart"), TEXT("spindle"), TEXT("folding"), TEXT("sling") })) return EHouseSurface::Metal;
    return EHouseSurface::Wood;
}

void HouseSfx::Impact(UWorld* World, const FVector& At, EHouseSurface Surface, float Strength, float MassKg)
{
    const float S = FMath::Clamp(Strength, 0.f, 1.f);
    const float Quiet = Surface == EHouseSurface::Soft ? .55f : (Surface == EHouseSurface::Paper ? .7f : 1.f);
    SfxPlay(World, SfxPcm(ESfxKind::Impact, Surface, SfxVariant(), SfxSizeBucket(MassKg)), At, (.2f + .9f * S) * Quiet, SfxPitch(), 160.f, 1400.f);
}

void HouseSfx::Break(UWorld* World, const FVector& At, EHouseSurface Surface, float MassKg, bool bCrackOnly)
{
    SfxPlay(World, SfxPcm(bCrackOnly ? ESfxKind::Crack : ESfxKind::Break, Surface, SfxVariant(), SfxSizeBucket(MassKg)), At,
        bCrackOnly ? .8f : 1.1f, SfxPitch(), 220.f, 2200.f);
}

void HouseSfx::Handle(UWorld* World, const FVector& At, int32 Kind, EHouseSurface Surface, float MassKg)
{
    const ESfxKind K = Kind == 0 ? ESfxKind::Pickup : (Kind == 1 ? ESfxKind::SetDown : ESfxKind::Throw);
    SfxPlay(World, SfxPcm(K, Surface, SfxVariant(), SfxSizeBucket(MassKg)), At, Kind == 1 ? .75f : .6f, SfxPitch(), 120.f, 900.f);
}

void HouseSfx::Click(UWorld* World, const FVector& At, int32 Kind)
{
    const ESfxKind K = Kind == 0 ? ESfxKind::Switch : (Kind == 1 ? ESfxKind::Pull : ESfxKind::Button);
    SfxPlay(World, SfxPcm(K, EHouseSurface::Wood, 0, 0), At, Kind == 1 ? .9f : .7f, 1.f, 120.f, 900.f);
}

UAudioComponent* HouseSfx::Song(USceneComponent* AttachTo)
{
    if (!AttachTo || !AttachTo->GetWorld() || AttachTo->GetWorld()->GetNetMode() == NM_DedicatedServer) return nullptr;
    const TArray<int16>& Pcm = SfxPcm(ESfxKind::Song, EHouseSurface::Wood, 0, 0);
    return UGameplayStatics::SpawnSoundAttached(SfxWave(Pcm), AttachTo, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true,
        .8f, 1.f, 0.f, SfxAttenuation(200.f, 1500.f), nullptr, true);
}
