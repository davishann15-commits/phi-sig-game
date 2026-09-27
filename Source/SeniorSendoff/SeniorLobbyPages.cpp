#include "SeniorLobbyPages.h"
#include "SeniorLobbyAtmosphere.h"
#include "SeniorSettingsPanel.h"
#include "SeniorLobby.h"
#include "SeniorCharacterPreview.h"
#include "SeniorCharacterRoster.h"
#include "SeniorCharacterProfiles.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "StoryCampaign.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameUserSettings.h"
#include "Styling/CoreStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace SeniorPages
{
const FLinearColor Cream(.94f, .92f, .87f, 1);
const FLinearColor Muted(.64f, .65f, .64f, 1);
const FLinearColor Accent(.86f, .31f, .11f, 1);
const FLinearColor Panel(.025f, .03f, .034f, .91f);
const FLinearColor ButtonFill(.065f, .073f, .079f, 1);
const FLinearColor SelectedFill(.29f, .105f, .045f, 1);
const TCHAR* Kits[] = { TEXT("BALANCED"), TEXT("HOUSE DEFENSE"), TEXT("LIGHTWEIGHT") };

FSlateFontInfo Font(int32 Size, bool bBold = false)
{
    return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
}

TSharedRef<STextBlock> Text(const FString& Value, int32 Size = 16,
    FLinearColor Color = Cream, bool bBold = false)
{
    return SNew(STextBlock).Text(FText::FromString(Value)).Font(Font(Size, bBold))
        .ColorAndOpacity(Color);
}

TSharedRef<STextBlock> Paragraph(const FString& Value, float Width, int32 Size = 16,
    FLinearColor Color = Muted)
{
    return SNew(STextBlock).Text(FText::FromString(Value)).Font(Font(Size))
        .ColorAndOpacity(Color).WrapTextAt(Width).LineHeightPercentage(1.15f);
}

const FButtonStyle& FlatButton()
{
    static const FButtonStyle Style = FButtonStyle()
        .SetNormal(FSlateColorBrush(FLinearColor::White))
        .SetHovered(FSlateColorBrush(FLinearColor(1.22f, 1.22f, 1.22f, 1)))
        .SetPressed(FSlateColorBrush(FLinearColor(.8f, .8f, .8f, 1)))
        .SetDisabled(FSlateColorBrush(FLinearColor(.4f, .4f, .4f, .5f)))
        .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    return Style;
}

const FButtonStyle& CharacterButton(bool Primary=false)
{
    static const FButtonStyle Dark=FButtonStyle()
        .SetNormal(FSlateRoundedBoxBrush(FLinearColor(.009f,.011f,.015f,.97f),2.f,FLinearColor(.12f,.13f,.15f,1),1.f))
        .SetHovered(FSlateRoundedBoxBrush(FLinearColor(.065f,.014f,.024f,1),2.f,FLinearColor(.6f,.07f,.11f,1),1.5f))
        .SetPressed(FSlateRoundedBoxBrush(FLinearColor(.035f,.006f,.012f,1),2.f,FLinearColor(.9f,.12f,.18f,1),1.5f))
        .SetDisabled(FSlateRoundedBoxBrush(FLinearColor(.015f,.018f,.02f,.9f),2.f,FLinearColor(.08f,.09f,.10f,1),1.f))
        .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0,1,0,-1));
    static const FButtonStyle Red=FButtonStyle(Dark)
        .SetNormal(FSlateRoundedBoxBrush(FLinearColor(.19f,.007f,.021f,1),2.f,FLinearColor(.55f,.055f,.09f,1),1.f))
        .SetHovered(FSlateRoundedBoxBrush(FLinearColor(.32f,.014f,.031f,1),2.f,FLinearColor(.85f,.1f,.15f,1),1.5f));
    return Primary ? Red : Dark;
}

TSharedRef<SWidget> CharacterPanel()
{
    static const FSlateRoundedBoxBrush Brush(FLinearColor(.009f,.012f,.016f,.95f),2.f,FLinearColor(.095f,.105f,.12f,1),1.f);
    return SNew(SBorder).Padding(0).BorderImage(&Brush);
}

void Place(const TSharedRef<SConstraintCanvas>& Canvas, float X, float Y,
    float Width, float Height, TSharedRef<SWidget> Widget)
{
    Canvas->AddSlot().Anchors(FAnchors(0, 0)).Alignment(FVector2D::ZeroVector)
        .Offset(FMargin(X, Y, Width, Height))[Widget];
}

TSharedRef<SBorder> Surface(FLinearColor Color = Panel)
{
    return SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(Color);
}

UGameUserSettings* Settings()
{
    return GEngine ? GEngine->GetGameUserSettings() : nullptr;
}

struct FNewsEntry
{
    const TCHAR* Label;
    const TCHAR* Title;
    const TCHAR* Summary;
    const TCHAR* Body;
};

const FNewsEntry News[] =
{
    { TEXT("PROTOTYPE UPDATE"), TEXT("A place for your party"),
      TEXT("A refreshed lobby and dedicated menu screens."),
      TEXT("Your party has a shared lobby with a full-body character lineup. Open Characters to browse the cast and rotate the character preview, then confirm your choice.\n\nCharacter and loadout choices update for the party. Changing either choice clears your ready status, so everyone can confirm before the story starts.\n\nOpen Settings to adjust graphics and frame rate, or check this News screen for project updates.") },
    { TEXT("PLAY TOGETHER"), TEXT("Room for three"),
      TEXT("Solo play or a party on the same local network."),
      TEXT("One player hosts a LAN lobby. Up to two friends on the same local network can join using the host's address. Open the lobby's Host / Join controls to get together.\n\nOnly connected players occupy the party slots. Every player must mark ready before the host can start or continue the story.\n\nPlaying alone works too: choose your character and loadout, mark ready, and start. LAN play requires the same game build on each device.") },
    { TEXT("STORY FOUNDATION"), TEXT("Three chapters, one story"),
      TEXT("Chapter travel, loading screens, and checkpoints."),
      TEXT("The campaign foundation connects three chapters: the house, the campus crossing, and the Fort House. Loading screens introduce the next location as the party travels.\n\nThe host can start a new story or continue an available saved checkpoint from the lobby. Starting a new story asks for confirmation before replacing the save.\n\nThese systems are the foundation for the game. The finished encounters, environments, weapons, and character abilities are still being developed.") },
    { TEXT("UPCOMING IDEAS"), TEXT("What's on the drawing board"),
      TEXT("The cast, house defense, and the senior bosses."),
      TEXT("PLANNED, NOT PLAYABLE YET\n\nThe cast will gain their own voices, personalities, and abilities as development continues.\n\nChapter 1 is planned around house defense: prep time, movable furniture, enemy waves, and Brady Miller's pledge summons. Later chapters will take the group across campus and into the Fort House.\n\nWeapons, enemy behavior, boss encounters, and character abilities will be built and tested as development continues. These are design plans, not a release announcement.") }
};
}

class SSeniorLobbyPage : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSeniorLobbyPage) {}
        SLATE_ARGUMENT(ASeniorLobbyController*, Controller)
        SLATE_ARGUMENT(ESeniorLobbyPage, Page)
        SLATE_EVENT(FSimpleDelegate, OnBack)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        Controller = Args._Controller;
        Page = Args._Page;
        if (Page == ESeniorLobbyPage::Loadout) Page = ESeniorLobbyPage::Character;
        OnBack = Args._OnBack;
        if (const auto* State = LocalState())
        {
            PreviewCharacter = FMath::Clamp(State->CharacterIndex, 0, SeniorRoster::Count - 1);
            PreviewWeapons[PreviewCharacter] = FMath::Clamp(State->LoadoutIndex,0,1);
        }
        using namespace SeniorPages;

#if WITH_EDITOR
        int32 TestCharacter=-1;
        if(FParse::Value(FCommandLine::Get(),TEXT("LobbyPreviewCharacter="),TestCharacter)) PreviewCharacter=FMath::Clamp(TestCharacter,0,SeniorRoster::Count-1);
        int32 TestWeapon=-1;
        if(FParse::Value(FCommandLine::Get(),TEXT("LobbyPreviewWeapon="),TestWeapon)) PreviewWeapons[PreviewCharacter]=FMath::Clamp(TestWeapon,0,1);
#endif
        auto Canvas = SNew(SConstraintCanvas);
        const bool bMainLobby = Page == ESeniorLobbyPage::MainLobby;
        if(Page != ESeniorLobbyPage::Character)
        {
        Place(Canvas, 60, 36, 950, 36, Text(bMainLobby ? TEXT("SENIOR SENDOFF  /  MAIN LOBBY") : TEXT("SENIOR SENDOFF  /  STORY LOBBY"), 17, Muted, true));
        Place(Canvas, 1228, 28, 312, 62, SNew(SButton).ButtonStyle(&CharacterButton())
            .ContentPadding(FMargin(22, 12))
            .HAlign(HAlign_Center)
            .OnClicked_Lambda([this]() { OnBack.ExecuteIfBound(); return FReply::Handled(); })
            [Text(bMainLobby ? TEXT("ENTER STORY LOBBY") : TEXT("BACK TO LOBBY"), 17, Cream, true)]);
        }

        const TCHAR* Title = TEXT("LOBBY");
        const TCHAR* Subtitle = TEXT("");
        switch (Page)
        {
            case ESeniorLobbyPage::Character:
                Title = TEXT("CHOOSE YOUR SURVIVOR");
                Subtitle = TEXT("Your character. Your weapon. Your way through the night.");
                break;
            case ESeniorLobbyPage::Loadout:
                Title = TEXT("YOUR LOADOUT");
                Subtitle = TEXT("Choose a preset for the story. Equipment and gameplay effects are still in development.");
                break;
            case ESeniorLobbyPage::Settings:
                Title = TEXT("SETTINGS");
                Subtitle = TEXT("Make the game feel right on this device.");
                break;
            case ESeniorLobbyPage::Achievements:
                Title = TEXT("ACHIEVEMENTS");
                Subtitle = TEXT("Story milestones are tracked from your local campaign save.");
                break;
            case ESeniorLobbyPage::MainLobby:
                Title = TEXT("MAIN LOBBY");
                Subtitle = TEXT("Your home before choosing a game mode. The full lobby will be designed here next.");
                break;
            case ESeniorLobbyPage::News:
                Title = TEXT("NEWS");
                Subtitle = TEXT("Project updates and a look at what's next.");
                break;
            case ESeniorLobbyPage::Credits:
                Title = TEXT("CREDITS");
                Subtitle = TEXT("The people, performances, and tools behind Senior Sendoff.");
                break;
            default: break;
        }
        if(Page != ESeniorLobbyPage::Character)
        {
        Place(Canvas, 60, 113, Page == ESeniorLobbyPage::Character ? 690 : 1475, 76,
            Text(Title, Page == ESeniorLobbyPage::Character ? 33 : 40, Cream, true));
        Place(Canvas, 63, 193, Page == ESeniorLobbyPage::Character ? 690 : 1475, 40,
            Text(Subtitle, 17, Muted));
        }

        switch (Page)
        {
            case ESeniorLobbyPage::Character: BuildCharacters(Canvas); break;
            case ESeniorLobbyPage::Loadout: BuildCharacters(Canvas); break;
            case ESeniorLobbyPage::Settings:
                Place(Canvas, 60, 250, 1480, 590, MakeSeniorSettingsPanel());
                break;
            case ESeniorLobbyPage::Achievements: BuildAchievements(Canvas); break;
            case ESeniorLobbyPage::MainLobby: BuildMainLobby(Canvas); break;
            case ESeniorLobbyPage::News: BuildNews(Canvas); break;
            case ESeniorLobbyPage::Credits: BuildCredits(Canvas); break;
            default: break;
        }
        ChildSlot[SNew(SOverlay)
            + SOverlay::Slot()[Surface(FLinearColor(.009f, .013f, .016f,
                (Page==ESeniorLobbyPage::Character || Page==ESeniorLobbyPage::Loadout)? .12f :
                (Page==ESeniorLobbyPage::MainLobby ? .62f : .91f)))]
            + SOverlay::Slot()[Canvas]];
    }

    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual void Tick(const FGeometry& Geometry,double Time,float Delta) override
    {
        SCompoundWidget::Tick(Geometry,Time,Delta);
        BrowseSlide = IsSeniorLobbyMotionEnabled() ? FMath::FInterpTo(BrowseSlide,0.f,Delta,14.f) : 0.f;
    }
    virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override
    {
        if (Event.GetKey() == EKeys::Escape)
        {
            OnBack.ExecuteIfBound();
            return FReply::Handled();
        }
        if (Page == ESeniorLobbyPage::Character &&
            (Event.GetKey() == EKeys::Left || Event.GetKey() == EKeys::Right))
        {
            Browse(Event.GetKey() == EKeys::Right ? 1 : -1);
            return FReply::Handled();
        }
        if(Page == ESeniorLobbyPage::Character && CanEdit())
        {
            if(Event.GetKey()==EKeys::One || Event.GetKey()==EKeys::Two)
            {
                PreviewWeapons[PreviewCharacter]=Event.GetKey()==EKeys::One ? 0 : 1;
                return FReply::Handled();
            }
            if(Event.GetKey()==EKeys::Enter) { ConfirmSelection(); return FReply::Handled(); }
        }
        return SCompoundWidget::OnKeyDown(Geometry, Event);
    }

private:
    TWeakObjectPtr<ASeniorLobbyController> Controller;
    ESeniorLobbyPage Page = ESeniorLobbyPage::Home;
    FSimpleDelegate OnBack;
    TStrongObjectPtr<UTexture2D> PortraitTextures[SeniorRoster::Count];
    FSlateBrush Portraits[SeniorRoster::Count];
    int32 PreviewCharacter = 0;
    int32 PreviewWeapons[SeniorRoster::Count] = {};
    float BrowseSlide = 0;
    TSharedPtr<SBox> NewsDetail;
    int32 SelectedNews = 0;
    FString SettingsFeedback = TEXT("Changes apply immediately and are saved on this device.");

    ASeniorLobbyPlayerState* LocalState() const
    {
        return Controller.IsValid() ? Controller->GetPlayerState<ASeniorLobbyPlayerState>() : nullptr;
    }

    bool CanEdit() const
    {
        if (!Controller.IsValid() || !LocalState() || !Controller->GetWorld()) return false;
        const auto* State = Controller->GetWorld()->GetGameState<ASeniorLobbyGameState>();
        const auto* Story = Controller->GetGameInstance<UStoryCampaign>();
        return State && !State->bStarting && (!Story || !Story->bTravelPending);
    }

    bool IsSelected(int32 Index, bool bCharacter) const
    {
        const auto* State = LocalState();
        return State && (bCharacter ? State->CharacterIndex : State->LoadoutIndex) == Index;
    }

    void Browse(int32 Direction)
    {
        if(!CanEdit()) return;
        PreviewCharacter = (PreviewCharacter + SeniorRoster::Count + Direction) % SeniorRoster::Count;
        BrowseSlide = Direction>0 ? 65.f : -65.f;
        FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
    }

    bool SelectionMatches() const
    {
        const auto* State = LocalState();
        return State && State->CharacterIndex == PreviewCharacter
            && State->LoadoutIndex == PreviewWeapons[PreviewCharacter];
    }

    void ConfirmSelection()
    {
        if(CanEdit() && !SelectionMatches())
        {
            Controller->SetCharacter(PreviewCharacter);
            Controller->SetLoadout(PreviewWeapons[PreviewCharacter]);
        }
    }

    void BuildCharacters(const TSharedRef<SConstraintCanvas>& Canvas)
    {
        using namespace SeniorPages;
        // No photographic backdrop or fixture-shaped Slate panels: this viewport
        // contains the character, architecture, world-space writing and shelf.
        Place(Canvas,0,0,1600,900,MakeSeniorCharacterRoomPreview(
            Controller.IsValid()?Controller->GetWorld():nullptr,
            TAttribute<int32>::CreateLambda([this](){return PreviewCharacter;}),
            TAttribute<int32>::CreateLambda([this](){return PreviewWeapons[PreviewCharacter];}),
            [this](int32 I){if(CanEdit())PreviewWeapons[PreviewCharacter]=I;},OnBack));
        for(int32 Direction:{-1,1})
            Place(Canvas,Direction<0?540:990,430,48,70,SNew(SButton).ButtonStyle(&CharacterButton())
                .IsEnabled_Lambda([this](){return CanEdit();}).HAlign(HAlign_Center).VAlign(VAlign_Center)
                .OnClicked_Lambda([this,Direction](){Browse(Direction);return FReply::Handled();})
                [Text(Direction<0?TEXT("‹"):TEXT("›"),36,Cream)]);
        Place(Canvas,540,771,520,30,SNew(STextBlock).Font(Font(18,true)).ColorAndOpacity(Cream).Justification(ETextJustify::Center)
            .Text_Lambda([this](){return FText::FromString(FString::Printf(TEXT("%s   /   %02d OF %02d"),*SeniorRoster::Label(PreviewCharacter),PreviewCharacter+1,SeniorRoster::Count));}));
        for(int32 I=0;I<SeniorRoster::Count;++I)
            Place(Canvas,658+I*34,811,25,7,SNew(SButton).ButtonStyle(&FlatButton()).ContentPadding(0)
                .IsEnabled_Lambda([this](){return CanEdit();})
                .ButtonColorAndOpacity_Lambda([this,I](){return I==PreviewCharacter?FLinearColor(.78,.16,.21,1):FLinearColor(.095,.11,.13,1);})
                .OnClicked_Lambda([this,I](){Browse(I-PreviewCharacter);return FReply::Handled();}));
        Place(Canvas,562,842,510,24,Text(TEXT("DRAG: ROTATE   •   SCROLL: ZOOM   •   RIGHT-DRAG: PAN"),10,Muted));
        Place(Canvas,1110,726,430,45,Text(TEXT("CHOOSE A WEAPON FROM THE SHELF"),11,Cream,true));
        Place(Canvas,1110,751,430,40,SNew(STextBlock).Font(Font(11)).ColorAndOpacity(Muted).WrapTextAt(420)
            .Text_Lambda([this](){return FText::FromString(PreviewCharacter==0 && PreviewWeapons[0]==0?
                TEXT("dǒulì • Returning throw"):TEXT("Prototype weapon • Gameplay in development"));}));
        Place(Canvas,1110,795,425,62,SNew(SButton).ButtonStyle(&CharacterButton(true)).HAlign(HAlign_Center).VAlign(VAlign_Center)
            .IsEnabled_Lambda([this](){return CanEdit()&&!SelectionMatches();})
            .OnClicked_Lambda([this](){ConfirmSelection();return FReply::Handled();})
            [SNew(STextBlock).Font(Font(17,true)).ColorAndOpacity(Cream)
                .Text_Lambda([this](){return FText::FromString(SelectionMatches()?TEXT("EQUIPPED FOR LOBBY"):TEXT("CONFIRM CHARACTER & WEAPON"));})]);
        Place(Canvas,1110,868,425,22,Text(TEXT("1 / 2  SELECT WEAPON     •     ENTER  CONFIRM"),10,Muted));
    }

    TSharedRef<SWidget> SettingsOption(const FString& Label, TFunction<bool()> IsActive,
        TFunction<void()> Action)
    {
        using namespace SeniorPages;
        return SNew(SButton).ButtonStyle(&FlatButton()).HAlign(HAlign_Center)
            .ContentPadding(FMargin(14, 15)).IsEnabled(Settings() != nullptr)
            .ButtonColorAndOpacity_Lambda([IsActive]() { return IsActive() ? SelectedFill : ButtonFill; })
            .OnClicked_Lambda([Action]() { Action(); return FReply::Handled(); })
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Text(Label, 17, Cream, true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0).HAlign(HAlign_Center)
                    [SNew(STextBlock).Font(Font(10, true)).ColorAndOpacity(Accent)
                        .Text_Lambda([IsActive]() {
                            return FText::FromString(IsActive() ? TEXT("SELECTED") : TEXT(""));
                        })]];
    }

    void BuildSettings(const TSharedRef<SConstraintCanvas>& Canvas)
    {
        using namespace SeniorPages;
        const TCHAR* Titles[] = { TEXT("GRAPHICS QUALITY"), TEXT("FRAME RATE LIMIT"), TEXT("V-SYNC"), TEXT("LOBBY ANIMATION") };
        const TCHAR* Descriptions[] =
        {
            TEXT("Choose the amount of visual detail."),
            TEXT("Set the maximum frames per second."),
            TEXT("Match frame presentation to the display to reduce tearing."),
            TEXT("Clouds, autumn leaves, and figures behind the windows. Turn off for reduced motion.")
        };
        for (int32 Row = 0; Row < 4; ++Row)
        {
            const float Y = 273 + Row * 132;
            Place(Canvas, 60, Y, 1480, 122, Surface());
            Place(Canvas, 88, Y + 13, 636, 32, Text(Titles[Row], 20, Cream, true));
            Place(Canvas, 89, Y + 51, 621, Row < 2 ? 27 : 62, Paragraph(Descriptions[Row], 601, 14));
            if (Row < 2)
            {
                Place(Canvas, 89, Y + 88, 621, 27,
                    SNew(STextBlock).Font(Font(13)).ColorAndOpacity(Cream)
                    .Text_Lambda([Row]() {
                        const auto* S = Settings();
                        if (!S) return FText::FromString(TEXT("Current setting unavailable"));
                        if (Row == 0)
                        {
                            const TCHAR* QualityNames[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") };
                            const int32 Quality = S->GetOverallScalabilityLevel();
                            const TCHAR* Name = Quality >= 0 && Quality < UE_ARRAY_COUNT(QualityNames) ? QualityNames[Quality] : TEXT("Custom");
                            return FText::FromString(FString(TEXT("Current: ")) + Name);
                        }
                        const float Rate = S->GetFrameRateLimit();
                        return FText::FromString(Rate <= 0 ? FString(TEXT("Current: Unlimited")) :
                            FString(TEXT("Current: ")) + FText::AsNumber(Rate).ToString() + TEXT(" FPS"));
                    }));
            }
            auto Options = SNew(SHorizontalBox);
            if (Row == 0)
            {
                const TCHAR* Names[] = { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH") };
                for (int32 I = 0; I < 3; ++I)
                    Options->AddSlot().FillWidth(1).Padding(6, 0)
                        [SettingsOption(Names[I], [I]() {
                            const auto* S = Settings(); return S && S->GetOverallScalabilityLevel() == I;
                        }, [this, I]() {
                            if (auto* S = Settings())
                            {
                                S->SetOverallScalabilityLevel(I); S->ApplySettings(false); S->SaveSettings();
                                const TCHAR* Values[] = { TEXT("Low"), TEXT("Medium"), TEXT("High") };
                                SettingsFeedback = FString::Printf(TEXT("Graphics quality set to %s. Saved on this device."), Values[I]);
                            }
                        })];
            }
            else if (Row == 1)
            {
                for (const int32 Rate : {30, 60})
                    Options->AddSlot().FillWidth(1).Padding(6, 0)
                        [SettingsOption(FString::Printf(TEXT("%d FPS"), Rate), [Rate]() {
                            const auto* S = Settings(); return S && FMath::IsNearlyEqual(S->GetFrameRateLimit(), float(Rate));
                        }, [this, Rate]() {
                            if (auto* S = Settings())
                            {
                                S->SetFrameRateLimit(Rate); S->ApplySettings(false); S->SaveSettings();
                                SettingsFeedback = FString::Printf(TEXT("Frame rate limit set to %d FPS. Saved on this device."), Rate);
                            }
                        })];
            }
            else
            {
                for (int32 I = 0; I < 2; ++I)
                {
                    const bool bEnabled = I == 1;
                    Options->AddSlot().FillWidth(1).Padding(6, 0)
                        [SettingsOption(bEnabled ? TEXT("ON") : TEXT("OFF"), [bEnabled, Row]() {
                            if (Row == 3) return IsSeniorLobbyMotionEnabled() == bEnabled;
                            const auto* S = Settings(); return S && S->IsVSyncEnabled() == bEnabled;
                        }, [this, bEnabled, Row]() {
                            if (Row == 3)
                            {
                                SetSeniorLobbyMotionEnabled(bEnabled);
                                SettingsFeedback = bEnabled ? TEXT("Lobby animation on. Saved on this device.")
                                    : TEXT("Lobby animation off for reduced motion. Saved on this device.");
                                return;
                            }
                            if (auto* S = Settings())
                            {
                                S->SetVSyncEnabled(bEnabled); S->ApplySettings(false); S->SaveSettings();
                                SettingsFeedback = FString::Printf(TEXT("V-Sync turned %s. Saved on this device."), bEnabled ? TEXT("on") : TEXT("off"));
                            }
                        })];
                }
            }
            Place(Canvas, 768, Y + 21, 748, 80, Options);
        }
        Place(Canvas, 64, 812, 1470, 43, SNew(STextBlock).Font(Font(16)).ColorAndOpacity(Muted)
            .Text_Lambda([this]() { return FText::FromString(SettingsFeedback); }));
    }

    UStoryCampaign* Story() const
    {
        return Controller.IsValid() ? Controller->GetGameInstance<UStoryCampaign>() : nullptr;
    }

    bool AchievementUnlocked(int32 Index) const
    {
        const UStoryCampaign* Campaign = Story();
        const UStorySave* Progress = Campaign ? Campaign->Progress.Get() : nullptr;
        if (!Progress) return false;
        switch (Index)
        {
            case 0: return Progress->bHasCheckpoint || Progress->Chapter > 1 || Progress->bCompleted;
            case 1: return Progress->Chapter > 1 || Progress->bCompleted;
            case 2: return Progress->Chapter > 2 || Progress->bCompleted;
            case 3: return Progress->bCompleted;
            default: return false;
        }
    }

    void BuildAchievements(const TSharedRef<SConstraintCanvas>& Canvas)
    {
        using namespace SeniorPages;
        static const TCHAR* Titles[] =
        {
            TEXT("FIRST CHECKPOINT"), TEXT("HOLD THE HOUSE"), TEXT("CROSS CAMPUS"),
            TEXT("SENIOR SENDOFF"), TEXT("FULL HOUSE"), TEXT("NO ONE LEFT BEHIND")
        };
        static const TCHAR* Descriptions[] =
        {
            TEXT("Reach your first story checkpoint."),
            TEXT("Complete Chapter 1: Defend the House."),
            TEXT("Complete Chapter 2: Cross the Campus."),
            TEXT("Finish all three chapters of Story Mode."),
            TEXT("Complete a chapter with a three-player party."),
            TEXT("Finish a chapter without losing a teammate.")
        };
        int32 Unlocked = 0;
        for (int32 I = 0; I < 4; ++I) Unlocked += AchievementUnlocked(I) ? 1 : 0;
        Place(Canvas,60,238,1480,44,SNew(STextBlock).Font(Font(14,true)).ColorAndOpacity(Accent)
            .Text(FText::FromString(FString::Printf(TEXT("STORY PROGRESS     %d / 4 UNLOCKED     •     2 MULTIPLAYER GOALS PLANNED"),Unlocked))));

        for (int32 I = 0; I < 6; ++I)
        {
            const int32 Column = I % 3;
            const int32 Row = I / 3;
            const float X = 60.f + Column * 495.f;
            const float Y = 300.f + Row * 247.f;
            Place(Canvas,X,Y,470,220,SNew(SBorder).Padding(FMargin(25,22))
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor_Lambda([this,I]() {
                    if(I>=4) return FLinearColor(.025f,.03f,.034f,.82f);
                    return AchievementUnlocked(I) ? FLinearColor(.11f,.035f,.039f,.95f) : FLinearColor(.025f,.03f,.034f,.91f);
                })
                [SNew(SVerticalBox)
                 +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font(11,true))
                    .ColorAndOpacity_Lambda([this,I]() { return I<4 && AchievementUnlocked(I) ? Accent : Muted; })
                    .Text_Lambda([this,I]() {
                        if(I>=4) return FText::FromString(TEXT("PLANNED ACHIEVEMENT"));
                        return FText::FromString(AchievementUnlocked(I) ? TEXT("UNLOCKED") : TEXT("LOCKED"));
                    })]
                 +SVerticalBox::Slot().AutoHeight().Padding(0,14,0,0)[Text(Titles[I],21,Cream,true)]
                 +SVerticalBox::Slot().AutoHeight().Padding(0,15,0,0)[Paragraph(Descriptions[I],410,14,I>=4?Muted:Cream)]
                 +SVerticalBox::Slot().FillHeight(1)
                 +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font(30,true)).Justification(ETextJustify::Right)
                    .ColorAndOpacity_Lambda([this,I]() { return I<4 && AchievementUnlocked(I) ? Accent : FLinearColor(.16f,.17f,.18f,1); })
                    .Text(FText::FromString(I>=4?TEXT("·"):TEXT("◆")))]]);
        }
        Place(Canvas,64,820,1470,34,Text(TEXT("Achievements are stored with this device's campaign progress."),13,Muted));
    }

    void BuildMainLobby(const TSharedRef<SConstraintCanvas>& Canvas)
    {
        using namespace SeniorPages;
        static const FSlateRoundedBoxBrush MainPanel(FLinearColor(.008f,.011f,.015f,.94f),3.f,FLinearColor(.26f,.07f,.08f,1),1.f);
        Place(Canvas,330,270,940,390,SNew(SBorder).Padding(FMargin(55,44)).BorderImage(&MainPanel)
            [SNew(SVerticalBox)
             +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Text(TEXT("SENIOR SENDOFF"),38,Cream,true)]
             +SVerticalBox::Slot().AutoHeight().Padding(0,13,0,0).HAlign(HAlign_Center)[Text(TEXT("MAIN LOBBY FOUNDATION"),13,Accent,true)]
             +SVerticalBox::Slot().AutoHeight().Padding(0,42,0,0).HAlign(HAlign_Center)
                [Paragraph(TEXT("Exit now returns here instead of closing the game. We can build the full main lobby, game-mode choices, and navigation on this screen next."),720,18,Cream)]
             +SVerticalBox::Slot().FillHeight(1)
             +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [SNew(SBox).WidthOverride(430).HeightOverride(68)
                 [SNew(SButton).ButtonStyle(&CharacterButton(true)).HAlign(HAlign_Center).VAlign(VAlign_Center)
                  .OnClicked_Lambda([this]() { OnBack.ExecuteIfBound(); return FReply::Handled(); })
                  [Text(TEXT("ENTER STORY LOBBY"),19,Cream,true)]]]]);
        Place(Canvas,510,690,580,30,SNew(STextBlock).Font(Font(12,true)).ColorAndOpacity(Muted)
            .Justification(ETextJustify::Center).Text(FText::FromString(TEXT("MAIN LOBBY DESIGN  /  NEXT DEVELOPMENT PASS"))));
    }

    void BuildCredits(const TSharedRef<SConstraintCanvas>& Canvas)
    {
        using namespace SeniorPages;
        static const FSlateRoundedBoxBrush CreditsPanel(FLinearColor(.018f,.022f,.027f,.94f),3.f,FLinearColor(.12f,.13f,.15f,1),1.f);
        static const TCHAR* Sections[] =
        {
            TEXT("CREATIVE TEAM"), TEXT("CHARACTERS & VOICES"), TEXT("SPECIAL THANKS")
        };
        static const TCHAR* Details[] =
        {
            TEXT("Creator, design, writing, art, and development credits will be added here as each role is finalized."),
            TEXT("The friends who inspired the cast, performed the voices, and helped bring each character to life."),
            TEXT("Playtesters, supporters, and everyone helping shape Senior Sendoff throughout development.")
        };

        Place(Canvas,60,255,1480,470,SNew(SBorder).Padding(FMargin(42,34)).BorderImage(&CreditsPanel)
            [SNew(SVerticalBox)
             +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Text(TEXT("SENIOR SENDOFF"),31,Cream,true)]
             +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,26).HAlign(HAlign_Center)[Text(TEXT("WORKING CREDITS"),12,Accent,true)]]);

        for(int32 I=0;I<3;++I)
        {
            const float X=95.f+I*480.f;
            Place(Canvas,X,365,430,245,SNew(SBorder).Padding(FMargin(25,22))
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor(.035f,.041f,.048f,.92f))
                [SNew(SVerticalBox)
                 +SVerticalBox::Slot().AutoHeight()[Text(Sections[I],14,Accent,true)]
                 +SVerticalBox::Slot().AutoHeight().Padding(0,18,0,0)[Paragraph(Details[I],375,16,Cream)]
                 +SVerticalBox::Slot().FillHeight(1)
                 +SVerticalBox::Slot().AutoHeight()[Text(TEXT("NAMES TO BE ADDED"),11,Muted,true)]]);
        }
        Place(Canvas,490,660,620,28,SNew(STextBlock).Font(Font(13,true)).ColorAndOpacity(Muted)
            .Justification(ETextJustify::Center).Text(FText::FromString(TEXT("CREATED WITH UNREAL ENGINE 5"))));
        Place(Canvas,60,780,1480,34,SNew(STextBlock).Font(Font(14)).ColorAndOpacity(Muted)
            .Justification(ETextJustify::Center)
            .Text(FText::FromString(TEXT("This credits page is ready for the final names, roles, performances, and acknowledgements."))));
    }

    void BuildNews(const TSharedRef<SConstraintCanvas>& Canvas)
    {
        using namespace SeniorPages;
        for (int32 I = 0; I < UE_ARRAY_COUNT(News); ++I)
        {
            Place(Canvas, 60, 269 + I * 144, 440, 130,
                SNew(SButton).ButtonStyle(&FlatButton()).ContentPadding(FMargin(22, 15))
                .ButtonColorAndOpacity_Lambda([this, I]() { return SelectedNews == I ? SelectedFill : ButtonFill; })
                .OnClicked_Lambda([this, I]() { SelectedNews = I; UpdateNewsDetail(); return FReply::Handled(); })
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[Text(News[I].Label, 10, Accent, true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)[Text(News[I].Title, 18, Cream, true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)[Paragraph(News[I].Summary, 384, 12)]]);
        }
        Place(Canvas, 531, 269, 1009, 562, SAssignNew(NewsDetail, SBox));
        UpdateNewsDetail();
        Place(Canvas, 64, 854, 1470, 30, Text(TEXT("Local project news. Upcoming ideas are not finished game features."), 13, Muted));
    }

    void UpdateNewsDetail()
    {
        using namespace SeniorPages;
        if (!NewsDetail.IsValid()) return;
        const auto& Entry = News[FMath::Clamp(SelectedNews, 0, int32(UE_ARRAY_COUNT(News)) - 1)];
        NewsDetail->SetContent(SNew(SBorder).Padding(FMargin(37, 29))
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Panel)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[Text(Entry.Label, 13, Accent, true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0)[Text(Entry.Title, 28, Cream, true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 28, 0, 0)[Paragraph(Entry.Body, 921, 16, Cream)]]);
    }
};

TSharedRef<SWidget> MakeSeniorLobbyPage(ASeniorLobbyController* Controller,
    ESeniorLobbyPage Page, FSimpleDelegate OnBack)
{
    return SNew(SSeniorLobbyPage).Controller(Controller).Page(Page).OnBack(OnBack);
}
