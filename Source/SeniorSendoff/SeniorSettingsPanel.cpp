#include "SeniorSettingsPanel.h"

#include "SeniorLobbyAtmosphere.h"
#include "SeniorPlayerPreferences.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
const FLinearColor Cream(.94f, .92f, .87f, 1);
const FLinearColor Muted(.66f, .68f, .69f, 1);
const FLinearColor Accent(.89f, .35f, .16f, 1);

FSlateFontInfo Font(int32 Size, bool bBold = false)
{
    return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
}

UGameUserSettings* VideoSettings()
{
    return GEngine ? GEngine->GetGameUserSettings() : nullptr;
}

bool IsQualityPreset(const UGameUserSettings* Settings, int32 Level)
{
    return Settings && Settings->GetViewDistanceQuality() == Level
        && Settings->GetAntiAliasingQuality() == Level
        && Settings->GetShadowQuality() == Level
        && Settings->GetGlobalIlluminationQuality() == Level
        && Settings->GetReflectionQuality() == Level
        && Settings->GetPostProcessingQuality() == Level
        && Settings->GetTextureQuality() == Level
        && Settings->GetVisualEffectQuality() == Level
        && Settings->GetFoliageQuality() == Level
        && Settings->GetShadingQuality() == Level
        && Settings->GetLandscapeQuality() == Level;
}

const FButtonStyle& OptionButtonStyle()
{
    static const FButtonStyle Style = FButtonStyle()
        .SetNormal(FSlateColorBrush(FLinearColor::White))
        .SetHovered(FSlateColorBrush(FLinearColor(1.16f, 1.16f, 1.16f, 1)))
        .SetPressed(FSlateColorBrush(FLinearColor(.82f, .82f, .82f, 1)))
        .SetDisabled(FSlateColorBrush(FLinearColor(.5f, .5f, .5f, .7f)))
        .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    return Style;
}
}

class SSeniorSettingsPanel final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSeniorSettingsPanel) {} SLATE_END_ARGS()

    void Construct(const FArguments&)
    {
        if (const UGameUserSettings* Video = VideoSettings())
        {
            const FIntPoint Native = Video->GetDesktopResolution();
            for (const FIntPoint Candidate : {FIntPoint(1280, 720), FIntPoint(1600, 900),
                FIntPoint(1920, 1080), FIntPoint(2560, 1440)})
            {
                if (Candidate.X <= Native.X && Candidate.Y <= Native.Y)
                    Resolutions.AddUnique(Candidate);
            }
            Resolutions.AddUnique(Native);
            Resolutions.AddUnique(Video->GetScreenResolution());
            Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
            { return int64(A.X) * A.Y < int64(B.X) * B.Y; });
        }

        TSharedRef<SScrollBox> Rows = SNew(SScrollBox)
            .Orientation(Orient_Vertical).ScrollBarAlwaysVisible(true);

        {
            auto Choices = SNew(SHorizontalBox);
            const TCHAR* Names[] = {TEXT("PERFORMANCE"), TEXT("BALANCED"), TEXT("QUALITY"), TEXT("EPIC")};
            for (int32 Level = 0; Level < 4; ++Level)
            {
                AddOption(Choices, Names[Level], [Level]() {
                    return IsQualityPreset(VideoSettings(), Level);
                }, [this, Level, Name = FString(Names[Level])]() {
                    if (auto* S = VideoSettings())
                    {
                        float Normalized, Current, Min, Max;
                        S->GetResolutionScaleInformationEx(Normalized, Current, Min, Max);
                        S->SetOverallScalabilityLevel(Level);
                        S->SetResolutionScaleValueEx(Current);
                        ApplyVideo(S, FString::Printf(TEXT("Graphics set to %s."), *Name));
                    }
                });
            }
            AddRow(Rows, TEXT("GRAPHICS PRESET"),
                TEXT("Adjusts shadows, effects, textures, and scene detail together."), Choices);
        }
        {
            auto Choices = SNew(SHorizontalBox);
            struct FMode { const TCHAR* Label; EWindowMode::Type Mode; };
            const FMode Modes[] = {
                {TEXT("FULLSCREEN"), EWindowMode::Fullscreen},
                {TEXT("BORDERLESS"), EWindowMode::WindowedFullscreen},
                {TEXT("WINDOWED"), EWindowMode::Windowed}
            };
            for (const FMode Choice : Modes)
            {
                AddOption(Choices, Choice.Label, [Mode = Choice.Mode]() {
                    const auto* S = VideoSettings(); return S && S->GetFullscreenMode() == Mode;
                }, [this, Mode = Choice.Mode, Name = FString(Choice.Label)]() {
                    if (auto* S = VideoSettings())
                    {
                        S->SetFullscreenMode(Mode);
                        if (Mode == EWindowMode::WindowedFullscreen)
                            S->SetScreenResolution(S->GetDesktopResolution());
                        ApplyVideo(S, FString::Printf(TEXT("Display mode set to %s."), *Name));
                    }
                });
            }
            AddRow(Rows, TEXT("DISPLAY MODE"),
                TEXT("Borderless is easiest for switching between the game and desktop."), Choices);
        }
        {
            auto Choices = SNew(SHorizontalBox);
            for (const FIntPoint Resolution : Resolutions)
            {
                const FString Name = FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y);
                AddOption(Choices, Name, [Resolution]() {
                    const auto* S = VideoSettings(); return S && S->GetScreenResolution() == Resolution;
                }, [this, Resolution, Name]() {
                    if (auto* S = VideoSettings())
                    {
                        S->SetScreenResolution(Resolution);
                        ApplyVideo(S, FString::Printf(TEXT("Resolution set to %s."), *Name));
                    }
                });
            }
            AddRow(Rows, TEXT("DISPLAY RESOLUTION"),
                TEXT("Choose a supported size up to this display's desktop resolution."), Choices);
        }
        {
            auto Choices = SNew(SHorizontalBox);
            for (const int32 Scale : {50, 67, 80, 100})
            {
                AddOption(Choices, FString::Printf(TEXT("%d%%"), Scale), [Scale]() {
                    const auto* S = VideoSettings();
                    if (!S) return false;
                    float Normalized, Current, Min, Max;
                    S->GetResolutionScaleInformationEx(Normalized, Current, Min, Max);
                    // Unreal uses zero to mean the project's native 100% default.
                    return FMath::IsNearlyEqual(Current <= 0.f ? 100.f : Current,
                        float(Scale), 2.0f);
                }, [this, Scale]() {
                    if (auto* S = VideoSettings())
                    {
                        S->SetResolutionScaleValueEx(Scale);
                        ApplyVideo(S, FString::Printf(TEXT("3D render scale set to %d%%."), Scale));
                    }
                });
            }
            AddRow(Rows, TEXT("3D RENDER SCALE"),
                TEXT("Lower values speed up the house while keeping menus sharp."), Choices);
        }
        {
            auto Choices = SNew(SHorizontalBox);
            for (const int32 Rate : {30, 60, 90, 120, 0})
            {
                AddOption(Choices, Rate ? FString::Printf(TEXT("%d FPS"), Rate) : TEXT("UNLIMITED"), [Rate]() {
                    const auto* S = VideoSettings(); return S && FMath::IsNearlyEqual(S->GetFrameRateLimit(), float(Rate));
                }, [this, Rate]() {
                    if (auto* S = VideoSettings())
                    {
                        S->SetFrameRateLimit(Rate);
                        ApplyVideo(S, Rate ? FString::Printf(TEXT("Frame rate capped at %d FPS."), Rate) : TEXT("Frame rate uncapped."));
                    }
                });
            }
            AddRow(Rows, TEXT("FRAME RATE LIMIT"),
                TEXT("A stable cap can make movement feel smoother on a busy scene."), Choices);
        }
        {
            auto Choices = SNew(SHorizontalBox);
            for (const bool bEnabled : {false, true})
            {
                AddOption(Choices, bEnabled ? TEXT("ON") : TEXT("OFF"), [bEnabled]() {
                    const auto* S = VideoSettings(); return S && S->IsVSyncEnabled() == bEnabled;
                }, [this, bEnabled]() {
                    if (auto* S = VideoSettings())
                    {
                        S->SetVSyncEnabled(bEnabled);
                        ApplyVideo(S, bEnabled ? TEXT("V-Sync on.") : TEXT("V-Sync off."));
                    }
                });
            }
            AddRow(Rows, TEXT("V-SYNC"),
                TEXT("Prevents tearing, but can add a little input delay."), Choices);
        }
        {
            auto Choices = SNew(SHorizontalBox);
            AddOption(Choices, TEXT("SLOWER"), []() { return false; }, [this]() {
                SeniorPlayerPreferences::SetMouseSensitivity(SeniorPlayerPreferences::Get().MouseSensitivity - .1f);
                Feedback = TEXT("Look sensitivity saved.");
            });
            Choices->AddSlot().FillWidth(.8f).VAlign(VAlign_Center).HAlign(HAlign_Center)
                [SNew(STextBlock).Font(Font(20, true)).ColorAndOpacity(Cream)
                    .Text_Lambda([]() {return FText::AsNumber(SeniorPlayerPreferences::Get().MouseSensitivity);})];
            AddOption(Choices, TEXT("FASTER"), []() { return false; }, [this]() {
                SeniorPlayerPreferences::SetMouseSensitivity(SeniorPlayerPreferences::Get().MouseSensitivity + .1f);
                Feedback = TEXT("Look sensitivity saved.");
            });
            AddRow(Rows, TEXT("MOUSE LOOK SENSITIVITY"),
                TEXT("Fine-tune how far the view turns for each mouse movement."), Choices);
        }
        {
            auto Choices = SNew(SHorizontalBox);
            for (const bool bEnabled : {false, true})
            {
                AddOption(Choices, bEnabled ? TEXT("INVERTED") : TEXT("NORMAL"), [bEnabled]() {
                    return SeniorPlayerPreferences::Get().bInvertY == bEnabled;
                }, [this, bEnabled]() {
                    SeniorPlayerPreferences::SetInvertY(bEnabled);
                    Feedback = TEXT("Vertical look preference saved.");
                });
            }
            AddRow(Rows, TEXT("VERTICAL LOOK"),
                TEXT("Choose whether moving the mouse upward looks up or down."), Choices);
        }
        {
            auto Choices = SNew(SHorizontalBox);
            for (const int32 Fov : {80, 90, 100, 110})
            {
                AddOption(Choices, FString::Printf(TEXT("%d°"), Fov), [Fov]() {
                    return FMath::IsNearlyEqual(SeniorPlayerPreferences::Get().FieldOfView, float(Fov));
                }, [this, Fov]() {
                    SeniorPlayerPreferences::SetFieldOfView(Fov);
                    Feedback = TEXT("Field of view saved; it updates while playing.");
                });
            }
            AddRow(Rows, TEXT("FIELD OF VIEW"),
                TEXT("Wider settings show more of the room at once."), Choices);
        }
        {
            auto Choices = SNew(SHorizontalBox);
            for (const bool bEnabled : {false, true})
            {
                AddOption(Choices, bEnabled ? TEXT("ON") : TEXT("OFF"), [bEnabled]() {
                    return IsSeniorLobbyMotionEnabled() == bEnabled;
                }, [this, bEnabled]() {
                    SetSeniorLobbyMotionEnabled(bEnabled);
                    Feedback = bEnabled ? TEXT("Lobby motion enabled.") : TEXT("Lobby motion reduced.");
                });
            }
            AddRow(Rows, TEXT("LOBBY ANIMATION"),
                TEXT("Turn off moving clouds, leaves, and figures in the lobby."), Choices);
        }

        ChildSlot[SNew(SVerticalBox)
            + SVerticalBox::Slot().FillHeight(1)[Rows]
            + SVerticalBox::Slot().AutoHeight().Padding(8, 12, 8, 0)
                [SNew(STextBlock).Font(Font(14)).ColorAndOpacity(Accent)
                    .Text_Lambda([this]() { return FText::FromString(Feedback); })]];
    }

private:
    struct FOption
    {
        FString Label;
        TFunction<bool()> IsActive;
        TFunction<void()> Action;
    };

    TArray<FIntPoint> Resolutions;
    FString Feedback = TEXT("Changes take effect immediately and are saved on this device.");

    void ApplyVideo(UGameUserSettings* Settings, const FString& Message)
    {
        Settings->ApplySettings(false);
        Settings->SaveSettings();
        Feedback = Message + TEXT(" Saved on this device.");
    }

    void AddOption(const TSharedRef<SHorizontalBox>& Choices, const FString& Label,
        TFunction<bool()> IsActive, TFunction<void()> Action)
    {
        const TSharedRef<FOption> Option = MakeShared<FOption>(FOption{Label, MoveTemp(IsActive), MoveTemp(Action)});
        Choices->AddSlot().FillWidth(1).Padding(4, 0).VAlign(VAlign_Center)
            [SNew(SButton).ButtonStyle(&OptionButtonStyle()).HAlign(HAlign_Center)
                .ContentPadding(FMargin(10, 11)).IsEnabled_Lambda([]() { return VideoSettings() != nullptr; })
                .ButtonColorAndOpacity_Lambda([Option]() {
                    return Option->IsActive() ? FLinearColor(.31f, .12f, .055f, 1) : FLinearColor(.055f, .066f, .076f, 1);
                })
                .OnClicked_Lambda([Option]() { Option->Action(); return FReply::Handled(); })
                [SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(13, true))
                    .ColorAndOpacity(Cream)]];
    }

    void AddRow(const TSharedRef<SScrollBox>& Rows, const FString& Title,
        const FString& Description, const TSharedRef<SWidget>& Choices)
    {
        static const FSlateColorBrush Panel(FLinearColor(.026f, .032f, .038f, .96f));
        Rows->AddSlot().Padding(0, 0, 13, 8)
            [SNew(SBox).HeightOverride(92)
                [SNew(SBorder).BorderImage(&Panel).Padding(FMargin(24, 12))
                    [SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(.43f).VAlign(VAlign_Center)
                            [SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight()
                                    [SNew(STextBlock).Text(FText::FromString(Title)).Font(Font(17, true)).ColorAndOpacity(Cream)]
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 12, 0)
                                    [SNew(STextBlock).Text(FText::FromString(Description)).Font(Font(12))
                                        .ColorAndOpacity(Muted).WrapTextAt(360)]]
                        + SHorizontalBox::Slot().FillWidth(.57f).VAlign(VAlign_Center)[Choices]]]];
    }
};

TSharedRef<SWidget> MakeSeniorSettingsPanel()
{
    return SNew(SSeniorSettingsPanel);
}
