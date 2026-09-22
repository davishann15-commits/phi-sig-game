#include "SeniorLobbyAtmosphere.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "Math/RandomStream.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SLeafWidget.h"

namespace
{
constexpr float ArtWidth = 1672.f, ArtHeight = 941.f;
#include "SeniorPhotoLeafData.inl"
constexpr int32 LeafCount = UE_ARRAY_COUNT(PhotoLeafSources);
bool bMotionLoaded = false, bMotionEnabled = true;
#if WITH_EDITOR
FSeniorLobbyAtmosphereDiagnostics Diagnostics;
TOptional<bool> TestMotionOverride;
TOptional<double> PhotoPreviewTime;
#endif

// Keep synchronized with the photo material: a 4.8-second peek every 30
// seconds. The material moves the original curtains and shades the
// figure behind their opening, so there is no walking-person overlay.
int32 WindowPeekCount(float Time)
{
    return Time >= 30.f && FMath::Fmod(Time - 30.f, 30.f) < 4.8f ? 1 : 0;
}

struct FHouseLightFlicker
{
    FRandomStream Random{91273};
    double Next[4] = {}, Last[4] = {-10,-10,-10,-10}, Previous = -1;
    FLinearColor Advance(double Time)
    {
        if (Previous < 0 || Time < Previous)
        {
            Random.Initialize(91273);
            for (int32 I=0; I<4; ++I)
            {
                Next[I] = Random.FRandRange(60.f,300.f);
                Last[I] = -10;
            }
        }
        Previous = Time;
        FLinearColor Dim(0,0,0,0);
        for (int32 I=0; I<4; ++I)
        {
            while (Time >= Next[I])
            {
                Last[I] = Next[I];
                Next[I] += Random.FRandRange(60.f,300.f);
            }
            const float Age = float(Time-Last[I]);
            // Two uneven dips, then full recovery. Exactly zero between events.
            const float A = FMath::SmoothStep(0.f,.12f,Age)*(1.f-FMath::SmoothStep(.32f,.62f,Age));
            const float B = FMath::SmoothStep(.95f,1.1f,Age)*(1.f-FMath::SmoothStep(1.5f,1.95f,Age));
            Dim.Component(I) = .70f*A+.87f*B;
        }
        return Dim;
    }
};

// Each moving mesh consists exclusively of pixels selected from the photo.
// The row-run geometry has holes wherever paving is visible between leaves.
struct FPhotoLeafState
{
    FVector2f Position, Velocity = FVector2f::ZeroVector;
    float Angle = 0, Spin = 0, Lift = 0;
};

struct FPhotoLeafMotion
{
    TArray<FPhotoLeafState> States;
    double Simulated = 0;

    void Reset()
    {
        States.SetNum(UE_ARRAY_COUNT(PhotoLeafSources));
        for (int32 I=0; I<States.Num(); ++I)
        {
            States[I] = FPhotoLeafState();
            States[I].Position = FVector2f(PhotoLeafSources[I].X,PhotoLeafSources[I].Y);
        }
        Simulated = 0;
    }

    static FVector2f Wind(double Time)
    {
        const float Age = FMath::Fmod(float(Time),30.f);
        const float Starts[] = {6,14,20,25};
        float Ordinary = 0;
        for (float Start : Starts)
        {
            const float T = Age-Start;
            if (T>=0) Ordinary = FMath::Max(Ordinary,
                FMath::SmoothStep(0.f,1.1f,T)*(1.f-FMath::SmoothStep(2.5f,4.f,T)));
        }
        const float Huge = Time>=30 ? FMath::SmoothStep(0.f,1.5f,Age)
            *(1.f-FMath::SmoothStep(4.f,6.f,Age)) : 0;
        return FVector2f(Ordinary,Huge);
    }

    void Advance(double Target)
    {
        if (States.IsEmpty() || Target+.0001 < Simulated) Reset();
        // Fixed steps also make frame captures identical to live playback.
        constexpr double Dt = 1.0/60.0;
        while (Simulated+Dt <= Target+.000001)
        {
            Simulated += Dt;
            const FVector2f Gust = Wind(Simulated);
            for (int32 I=0; I<States.Num(); ++I)
            {
                auto& S = States[I];
                const auto& Home = PhotoLeafSources[I];
                const float Seed = I*2.399963f;
                const float Depth = FMath::Clamp((Home.Y-690.f)/251.f,.15f,1.f);
                const float Mobility = FMath::Clamp(7.f/FMath::Sqrt(Home.Area),.45f,1.4f);
                const float Resistance = (I%6==0?90.f:8.f)+FMath::Sqrt(Home.Area)*1.2f;
                const float Push = FMath::Max(0.f,105.f*Gust.X+520.f*Gust.Y-Resistance)*Mobility*(.3f+.7f*Depth);
                const float Drag = 5.f-2.5f*Gust.Y;
                const FVector2f Force(Push,Push*(.035f+.045f*FMath::Sin(Seed+float(Simulated)*.6f)));
                S.Velocity += (Force/Drag-S.Velocity)*(1.f-FMath::Exp(-Drag*float(Dt)));
                S.Position += S.Velocity*float(Dt);
                const float SpinTarget=(Gust.X*.7f*FMath::Sin(float(Simulated)*2.f+Seed)+Gust.Y*2.8f)*Mobility;
                S.Spin += (SpinTarget-S.Spin)*(1.f-FMath::Exp(-3.f*float(Dt)));
                if (Home.Area>150.f)
                {
                    const float AngleTarget=.12f*FMath::Sin(float(Simulated)*1.4f+Seed)
                        *FMath::Max(Gust.X,Gust.Y);
                    S.Angle += (AngleTarget-S.Angle)*(1.f-FMath::Exp(-3.f*float(Dt)));
                }
                else S.Angle += S.Spin*float(Dt);
                float LiftTarget = (Gust.X*(1.f+Depth*3.f)+Gust.Y*(5.f+Depth*22.f))*Mobility
                    *FMath::Pow(FMath::Max(0.f,FMath::Sin(float(Simulated)*(2.2f+I%3*.2f)+Seed)),2.f);
                LiftTarget *= FMath::Clamp(100.f/Home.Area,.12f,1.f);
                S.Lift += (LiftTarget-S.Lift)*(1.f-FMath::Exp(-6.f*float(Dt)));
                const float Margin = FMath::Sqrt(Home.Area)*2.f+20.f;
                // Recycle the same photographed leaves only once fully outside
                // the view. No new shapes and no on-screen resets or fades.
                if (S.Position.X>ArtWidth+Margin || S.Position.Y>ArtHeight+Margin)
                {
                    S.Position=FVector2f(-Margin,Home.Y);
                }
            }
        }
    }

    void Draw(const FGeometry& Geometry,FSlateWindowElementList& Out,int32 Layer,
        const FSlateBrush& Photo) const
    {
        if (States.Num()!=UE_ARRAY_COUNT(PhotoLeafSources)) return;
        const FVector2f Scale=FVector2f(Geometry.GetLocalSize())/FVector2f(ArtWidth,ArtHeight);
        TArray<FSlateVertex> ShadowVertices,MovingVertices;
        TArray<SlateIndex> Indices;
        ShadowVertices.Reserve(UE_ARRAY_COUNT(PhotoLeafRects)*4);
        MovingVertices.Reserve(UE_ARRAY_COUNT(PhotoLeafRects)*4);
        Indices.Reserve(UE_ARRAY_COUNT(PhotoLeafRects)*6);
        for (int32 I=0; I<States.Num(); ++I)
        {
            const auto& Home=PhotoLeafSources[I]; const auto& S=States[I];
            const float C=FMath::Cos(S.Angle),Sin=FMath::Sin(S.Angle);
            // Rotate on the ground plane, then project back into this camera.
            // A flat photographed leaf must not become a tall vertical strip.
            const float Perspective=.25f+.20f*FMath::Clamp((Home.Y-714.f)/227.f,0.f,1.f);
            const float Gentle=(I%6==0?.035f:.22f)*(FMath::Sin(float(Simulated)*.7f+I)-FMath::Sin(float(I)));
            for (int32 R=Home.First; R<Home.First+Home.Count; ++R)
            {
                const auto& Rect=PhotoLeafRects[R];
                const FVector2f Points[]={{float(Rect.X1),float(Rect.Y1)},{float(Rect.X2),float(Rect.Y1)},
                    {float(Rect.X2),float(Rect.Y2)},{float(Rect.X1),float(Rect.Y2)}};
                const SlateIndex Base=MovingVertices.Num();
                for (const FVector2f& P:Points)
                {
                    const FVector2f UV=P/FVector2f(ArtWidth,ArtHeight);
                    const FVector2f Local=P-FVector2f(Home.X,Home.Y);
                    const FVector2f M=S.Position+FVector2f(C*Local.X-Sin*Local.Y/Perspective+Gentle,
                        Sin*Local.X*Perspective+C*Local.Y-S.Lift);
                    const FVector2f Shadow=M+FVector2f(1.2f,S.Lift+1.f);
                    ShadowVertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
                        Geometry.GetAccumulatedRenderTransform(),Shadow*Scale,UV,
                        FColor(0,0,0,uint8(30.f/(1.f+S.Lift*.04f)))));
                    MovingVertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
                        Geometry.GetAccumulatedRenderTransform(),M*Scale,UV,FColor::White));
                }
                Indices.Append({Base,SlateIndex(Base+1),SlateIndex(Base+2),Base,SlateIndex(Base+2),SlateIndex(Base+3)});
            }
        }
        auto Renderer=FSlateApplication::Get().GetRenderer();
        FSlateDrawElement::MakeCustomVerts(Out,Layer,Renderer->GetResourceHandle(Photo),ShadowVertices,Indices,nullptr,0,0);
        FSlateDrawElement::MakeCustomVerts(Out,Layer+1,Renderer->GetResourceHandle(Photo),MovingVertices,Indices,nullptr,0,0);
    }
};


class SSeniorLobbyAtmosphere final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SSeniorLobbyAtmosphere) : _Active(true) {}
        SLATE_ATTRIBUTE(bool, Active)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        Active = Args._Active;
        SetVisibility(EVisibility::HitTestInvisible);
        SetCanTick(true);
        ForceVolatile(true);
        SetClipping(EWidgetClipping::ClipToBounds);
        Backdrop.Reset(LoadObject<UTexture2D>(nullptr, TEXT("/Game/Lobby/UI/T_LobbyBackdrop.T_LobbyBackdrop")));
        Brush.SetResourceObject(Backdrop.Get());
        Brush.ImageSize = FVector2D(ArtWidth, ArtHeight);
        Brush.DrawAs = ESlateBrushDrawType::Image;
        if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Lobby/UI/M_LobbyAtmosphere.M_LobbyAtmosphere")))
        {
            Sky.Reset(UMaterialInstanceDynamic::Create(Material, GetTransientPackage()));
            Sky->SetScalarParameterValue(TEXT("AmbientSeconds"), 0.f);
            Sky->SetScalarParameterValue(TEXT("LeafWindStrength"), 0.f);
            Sky->SetVectorParameterValue(TEXT("HouseLightDim"), FLinearColor(0,0,0,0));
            Brush.SetResourceObject(Sky.Get());
        }
        else UE_LOG(LogTemp, Warning, TEXT("Lobby atmosphere sky material missing; using original backdrop."));
        PhotoBrush.SetResourceObject(Backdrop.Get());
        PhotoLeaves.Reset();
    }

    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(ArtWidth, ArtHeight); }

    virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override
    {
        SLeafWidget::Tick(Geometry, CurrentTime, DeltaTime);
        bAnimating = Active.Get() && IsSeniorLobbyMotionEnabled();
        if (!bAnimating) return;
#if WITH_EDITOR
        if (PhotoPreviewTime.IsSet()) Seconds = PhotoPreviewTime.GetValue();
        else
#endif
            Seconds += FMath::Clamp(double(DeltaTime), 0.0, .1);
        PhotoLeaves.Advance(Seconds);
        if (Sky.IsValid() && Seconds != LastMaterialUpdate)
        {
            Sky->SetScalarParameterValue(TEXT("AmbientSeconds"), float(Seconds));
            const FVector2f Gust = FPhotoLeafMotion::Wind(Seconds);
            Sky->SetScalarParameterValue(TEXT("LeafWindStrength"), .55f*Gust.X+Gust.Y);
            Sky->SetVectorParameterValue(TEXT("HouseLightDim"), HouseLights.Advance(Seconds));
            LastMaterialUpdate = Seconds;
        }
    }

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
        FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
    {
        FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(), &Brush);
        const int32 People = Sky.IsValid() ? WindowPeekCount(float(Seconds)) : 0;
        if (Sky.IsValid() && Backdrop.IsValid())
            PhotoLeaves.Draw(Geometry,Out,Layer+1,PhotoBrush);
#if WITH_EDITOR
        Diagnostics.Seconds = Seconds;
        ++Diagnostics.Paints;
        Diagnostics.Leaves = LeafCount;
        Diagnostics.WindowPassers = People;
        Diagnostics.bSkyMaterial = Sky.IsValid();
        Diagnostics.bAnimating = bAnimating;
#endif
        return Layer + 3;
    }

private:
    TAttribute<bool> Active;
    TStrongObjectPtr<UTexture2D> Backdrop;
    TStrongObjectPtr<UMaterialInstanceDynamic> Sky;
    FSlateBrush Brush, PhotoBrush;
    FPhotoLeafMotion PhotoLeaves;
    double Seconds = 0, LastMaterialUpdate = 0;
    FHouseLightFlicker HouseLights;
    bool bAnimating = false;
};
}

bool IsSeniorLobbyMotionEnabled()
{
#if WITH_EDITOR
    if (TestMotionOverride.IsSet()) return TestMotionOverride.GetValue();
#endif
    if (!bMotionLoaded)
    {
        if (GConfig) GConfig->GetBool(TEXT("SeniorLobbyAtmosphere"), TEXT("bMotionEnabled"), bMotionEnabled, GGameUserSettingsIni);
        bMotionLoaded = true;
    }
    return bMotionEnabled;
}

void SetSeniorLobbyMotionEnabled(bool Enabled)
{
    bMotionLoaded = true; bMotionEnabled = Enabled;
    if (GConfig)
    {
        GConfig->SetBool(TEXT("SeniorLobbyAtmosphere"), TEXT("bMotionEnabled"), Enabled, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }
}

TSharedRef<SWidget> MakeSeniorLobbyAtmosphere(TAttribute<bool> Active)
{
    return SNew(SSeniorLobbyAtmosphere).Active(Active);
}

#if WITH_EDITOR
FSeniorLobbyAtmosphereDiagnostics GetSeniorLobbyAtmosphereDiagnostics() { return Diagnostics; }
void SetSeniorLobbyMotionTestOverride(TOptional<bool> Enabled) { TestMotionOverride = Enabled; }
void SetSeniorLobbyPhotoPreviewTime(TOptional<double> Seconds) { PhotoPreviewTime = Seconds; }
#endif
