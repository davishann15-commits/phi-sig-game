#include "SeniorLobbyUI.h"
#include "SeniorSelectionRoom.h"
#include "SeniorLobby.h"
#include "SeniorLobbyPages.h"
#include "SeniorLobbyAtmosphere.h"
#include "SeniorCharacterPreview.h"
#include "SeniorCharacterRoster.h"
#include "StoryCampaign.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Colors/SSimpleGradient.h"

namespace SeniorUI
{
const FLinearColor White(.94f, .92f, .88f, 1);
const FLinearColor Muted(.57f, .61f, .65f, 1);
const FLinearColor Orange(.9f, .23f, .065f, 1);
const FLinearColor Panel(.018f, .021f, .028f, .94f);
const FLinearColor Green(.17f, .72f, .38f, 1);
const TCHAR* Kits[] = { TEXT("BALANCED"), TEXT("HOUSE DEFENSE"), TEXT("LIGHTWEIGHT") };

FSlateFontInfo Font(int32 Size, bool Bold = false)
{
    return FCoreStyle::GetDefaultFontStyle(Bold ? "Bold" : "Regular", Size);
}
TSharedRef<STextBlock> Label(const FString& Text, int32 Size = 16, FLinearColor Color = White, bool Bold = false)
{
    return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(Size, Bold)).ColorAndOpacity(Color);
}
const FButtonStyle& LobbyButtonStyle(bool Accent = false)
{
    // Persistent brushes: charcoal panels, restrained copper outlines, and
    // distinct press/disabled states without the editor's glossy default skin.
    static const FButtonStyle Secondary = FButtonStyle()
        .SetNormal(FSlateRoundedBoxBrush(FLinearColor(.009f,.011f,.014f,.97f),1.f,FLinearColor(.12f,.105f,.095f,1),1.f))
        .SetHovered(FSlateRoundedBoxBrush(FLinearColor(.055f,.018f,.021f,1),1.f,FLinearColor(.58f,.055f,.065f,1),1.5f))
        .SetPressed(FSlateRoundedBoxBrush(FLinearColor(.025f,.007f,.01f,1),1.f,FLinearColor(.9f,.12f,.14f,1),1.5f))
        .SetDisabled(FSlateRoundedBoxBrush(FLinearColor(.025f,.027f,.03f,.8f),4.f,FLinearColor(.09f,.095f,.10f,1),1.f))
        .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0,1,0,-1));
    static const FButtonStyle Primary = FButtonStyle(Secondary)
        .SetNormal(FSlateRoundedBoxBrush(FLinearColor(.13f,.006f,.009f,.96f),1.f,FLinearColor(.49f,.09f,.10f,1),1.f))
        .SetHovered(FSlateRoundedBoxBrush(FLinearColor(.28f,.012f,.019f,1),1.f,FLinearColor(.85f,.15f,.18f,1),1.5f))
        .SetPressed(FSlateRoundedBoxBrush(FLinearColor(.23f,.004f,.011f,1),1.f,FLinearColor(.7f,.045f,.065f,1),1.f))
        .SetDisabled(FSlateRoundedBoxBrush(FLinearColor(.095f,.009f,.015f,.94f),1.f,FLinearColor(.22f,.025f,.038f,1),1.f));
    return Accent ? Primary : Secondary;
}
const FButtonStyle& MenuRowStyle()
{
    static const FButtonStyle Style = FButtonStyle()
        .SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent,0.f))
        .SetHovered(FSlateRoundedBoxBrush(FLinearColor(.12f,.014f,.020f,.85f),0.f))
        .SetPressed(FSlateRoundedBoxBrush(FLinearColor(.065f,.005f,.010f,.95f),0.f))
        .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0,1,0,-1));
    return Style;
}
TSharedRef<SWidget> Button(const FString& Text, TFunction<void()> Action, bool Accent = false)
{
    return SNew(SButton).ContentPadding(FMargin(18, 12))
        .ButtonStyle(&LobbyButtonStyle(Accent)).HAlign(HAlign_Center).VAlign(VAlign_Center)
        .OnClicked_Lambda([Action]() { Action(); return FReply::Handled(); })
        [Label(Text, 17, White, true)];
}
void Place(const TSharedRef<SConstraintCanvas>& Canvas, float X, float Y, float W, float H, TSharedRef<SWidget> Widget)
{
    Canvas->AddSlot().Anchors(FAnchors(0,0)).Alignment(FVector2D::ZeroVector).Offset(FMargin(X,Y,W,H))[Widget];
}
void InitBrush(FSlateBrush& Brush, UTexture2D* Texture)
{
    Brush.SetResourceObject(Texture);
    Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.ImageSize = FVector2D(1672, 941);
}
}

class SSeniorLobbyWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSeniorLobbyWidget) {}
        SLATE_ARGUMENT(ASeniorLobbyController*, Controller)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        Controller = Args._Controller;
        CharacterBackdrop.Reset(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Lobby/UI/T_CharacterBackdropWide.T_CharacterBackdropWide")));
        SeniorUI::InitBrush(CharacterBackdropBrush,CharacterBackdrop.Get());
        if(CharacterBackdrop.IsValid()) CharacterBackdropBrush.ImageSize=FVector2D(CharacterBackdrop->GetSizeX(),CharacterBackdrop->GetSizeY());
#if WITH_EDITOR
        bBackdropOnly = FParse::Param(FCommandLine::Get(), TEXT("LobbyBackdropOnly"));
#endif
        auto Canvas = SNew(SConstraintCanvas);
        using namespace SeniorUI;
        FSlateFontInfo TitleFont(FPaths::EngineContentDir()/TEXT("Slate/Fonts/Roboto-BoldCondensed.ttf"),58);
        TitleFont.LetterSpacing=35;
        TitleFont.OutlineSettings.OutlineSize=2;
        TitleFont.OutlineSettings.OutlineColor=FLinearColor(.012f,.006f,.007f,.95f);
        Place(Canvas,50,25,880,84,SNew(STextBlock)
            .Text(FText::FromString(TEXT("SENIOR SENDOFF"))).Font(TitleFont)
            .ColorAndOpacity(FLinearColor(.87f,.845f,.79f,1))
            .ShadowOffset(FVector2D(3,4)).ShadowColorAndOpacity(FLinearColor(.24f,.009f,.018f,.95f)));
        Place(Canvas,50,112,565,3,SNew(SSimpleGradient)
            .Orientation(Orient_Vertical).HasAlphaBackground(false)
            .StartColor(FLinearColor(.62f,.025f,.04f,1))
            .EndColor(FLinearColor(.26f,.008f,.014f,0)));
        FSlateFontInfo SubtitleFont=Font(16,true); SubtitleFont.LetterSpacing=240;
        Place(Canvas,53,124,500,30,SNew(STextBlock).Text(FText::FromString(TEXT("STORY MODE")))
            .Font(SubtitleFont).ColorAndOpacity(FLinearColor(.72f,.69f,.65f,1))
            .ShadowOffset(FVector2D(1,2)).ShadowColorAndOpacity(FLinearColor(0,0,0,.9f)));
        static const FSlateRoundedBoxBrush MenuPanel(FLinearColor(.009f,.010f,.013f,.92f),1.f,FLinearColor(.12f,.125f,.13f,1),1.f);
        Place(Canvas,50,220,336,385,SNew(SBorder).BorderImage(&MenuPanel).Padding(0));
        const TCHAR* Tabs[] = {TEXT("HOST PARTY"),TEXT("CHARACTER"),TEXT("SETTINGS"),TEXT("ACHIEVEMENTS"),TEXT("NEWS"),TEXT("CREDITS")};
        const ESeniorLobbyPage Pages[] = {ESeniorLobbyPage::Home,ESeniorLobbyPage::Character,
            ESeniorLobbyPage::Settings,ESeniorLobbyPage::Achievements,ESeniorLobbyPage::News,ESeniorLobbyPage::Credits};
        for(int32 I=0; I<6; ++I)
        {
            const ESeniorLobbyPage Page=Pages[I];
            auto Row = SNew(SButton).ContentPadding(0)
                .ButtonStyle(&MenuRowStyle()).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
                .OnClicked_Lambda([this,Page,I]() {
                    if(I==0) ShowNetwork(); else OpenPage(Page);
                    return FReply::Handled();
                });
            TWeakPtr<SButton> WeakRow = Row;
            Row->SetContent(SNew(SOverlay)
                +SOverlay::Slot().VAlign(VAlign_Top).Padding(12,0)
                    [SNew(SBox).HeightOverride(1)[SNew(SBorder).Padding(0)
                     .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.10f,.10f,.105f,.65f))]]
                +SOverlay::Slot().HAlign(HAlign_Left)
                    [SNew(SBox).WidthOverride(6)[SNew(SBorder).Padding(0)
                     .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                     .BorderBackgroundColor_Lambda([WeakRow]() {
                         auto Button=WeakRow.Pin();
                         return Button.IsValid() && (Button->IsHovered() || Button->HasKeyboardFocus())
                             ? FLinearColor(.85f,.035f,.06f,1) : FLinearColor::Transparent;
                     })]]
                +SOverlay::Slot().VAlign(VAlign_Center).Padding(26,0,12,0)[Label(Tabs[I],19,White,true)]
                +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0,0,18,0)
                    [SNew(STextBlock).Font(Font(11,true)).ColorAndOpacity(Muted)
                     .Visibility(I==0?EVisibility::HitTestInvisible:EVisibility::Collapsed)
                     .Text_Lambda([this]() {
                         const bool LAN=Controller.IsValid() && Controller->GetNetMode()!=NM_Standalone;
                         return FText::FromString(FString::Printf(TEXT("%s  %d / 3"),LAN?TEXT("LAN"):TEXT("SOLO"),Members().Num()));
                     })]);
            Place(Canvas,51,221+I*64,334,64,Row);
        }
        Place(Canvas,50,803,336,58,SNew(SButton).ContentPadding(FMargin(24,10))
            .ButtonStyle(&LobbyButtonStyle()).HAlign(HAlign_Left).VAlign(VAlign_Center)
            .OnClicked_Lambda([this]() { OpenPage(ESeniorLobbyPage::MainLobby); return FReply::Handled(); })
            [SNew(SHorizontalBox)
             +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(TEXT("‹"),30,FLinearColor(.82f,.18f,.21f,1),true)]
             +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14,0,0,0)[Label(TEXT("EXIT"),18,White,true)]]);
        for(int32 I=0; I<3; ++I) Place(Canvas,427+I*375,257,360,516,MakeStandingPlayer(I));

        Place(Canvas,438,778,1095,24,SNew(STextBlock).Font(Font(12)).ColorAndOpacity(White)
            .Text_Lambda([this]() { return FText::FromString(StatusText()); }));
        Place(Canvas,438,795,1095,66,SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(0,0,12,0)
                [SNew(SBox).WidthOverride(260)
                 .Visibility_Lambda([this]() { return Members().Num()>1 ? EVisibility::Visible : EVisibility::Collapsed; })
                 [SNew(SButton).ContentPadding(FMargin(18,8))
                  .ButtonStyle(&LobbyButtonStyle()).HAlign(HAlign_Center).VAlign(VAlign_Center)
                  .IsEnabled_Lambda([this]() { return Controller.IsValid() && LocalState() && !Starting(); })
                  .OnClicked_Lambda([this]() {
                      if(Controller.IsValid() && LocalState()) Controller->SetReady(!LocalState()->bReady);
                      return FReply::Handled();
                  })
                  [SNew(STextBlock).Font(Font(19,true)).ColorAndOpacity(White).Text_Lambda([this]() {
                      return FText::FromString(LocalState() && LocalState()->bReady ? TEXT("CANCEL READY") : TEXT("READY UP"));
                  })]]]
            +SHorizontalBox::Slot().FillWidth(1)
                [SNew(SButton).ContentPadding(FMargin(22,8))
                 .ButtonStyle(&LobbyButtonStyle(true)).HAlign(HAlign_Center).VAlign(VAlign_Center)
                 .IsEnabled_Lambda([this]() { return CanStart(); })
                 .OnClicked_Lambda([this]() { ShowStartGameMenu(); return FReply::Handled(); })
                 [SNew(STextBlock).Font(Font(25,true)).ColorAndOpacity(White).Text_Lambda([this]() {
                     if(Starting()) return FText::FromString(TEXT("STARTING..."));
                     return FText::FromString(Controller.IsValid() && !Controller->IsHost() ? TEXT("WAITING FOR HOST") : TEXT("START GAME"));
                 })]]);
        ChildSlot[SNew(SOverlay)
            +SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFill)
                [MakeSeniorLobbyAtmosphere(TAttribute<bool>::CreateLambda([this]() {
                    return CurrentPage == ESeniorLobbyPage::Home && !Starting()
                        && (!Modal.IsValid() || Modal->GetVisibility() == EVisibility::Collapsed);
                }))]]
            +SOverlay::Slot()[SNew(SBorder).Padding(0)
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor(.025f,.027f,.029f,1))
                .Visibility_Lambda([this](){return CharacterBackdrop.IsValid() && (CurrentPage==ESeniorLobbyPage::Character || CurrentPage==ESeniorLobbyPage::Loadout)?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
                [SNew(SBox)]]
            +SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0,0,0,.12f))]
            +SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1600).HeightOverride(900)
                [SNew(SOverlay)
                 +SOverlay::Slot()[SNew(SBox).Visibility_Lambda([this](){return CurrentPage==ESeniorLobbyPage::Home && !bBackdropOnly?EVisibility::SelfHitTestInvisible:EVisibility::Collapsed;})[Canvas]]
                 +SOverlay::Slot()[SAssignNew(PageContent,SBox).Visibility(EVisibility::Collapsed)]]]]
            +SOverlay::Slot()[SAssignNew(Modal,SBox).Visibility(EVisibility::Collapsed)]
            +SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor(.006f,.009f,.013f,.97f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
                .Visibility_Lambda([this](){return Starting()?EVisibility::Visible:EVisibility::Collapsed;})
                [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Label(TEXT("PREPARING YOUR STORY"),30,White,true)]
                 +SVerticalBox::Slot().AutoHeight().Padding(0,24).HAlign(HAlign_Center)[SNew(SThrobber)]]]];
#if WITH_EDITOR
        FString PreviewPage;
        if(FParse::Value(FCommandLine::Get(),TEXT("LobbyPreviewPage="),PreviewPage))
        {
            if(PreviewPage.Equals(TEXT("Character"),ESearchCase::IgnoreCase) || PreviewPage.Equals(TEXT("Loadout"),ESearchCase::IgnoreCase)) OpenPage(ESeniorLobbyPage::Character);
            else if(PreviewPage.Equals(TEXT("MainLobby"),ESearchCase::IgnoreCase)) OpenPage(ESeniorLobbyPage::MainLobby);
            else for(int32 I=0;I<6;++I) if(PreviewPage.Equals(Tabs[I],ESearchCase::IgnoreCase)) OpenPage(Pages[I]);
        }
        if(FParse::Param(FCommandLine::Get(),TEXT("LobbyPreviewStartMenu"))) ShowStartGameMenu();
#endif
    }

    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override
    {
        if(Event.GetKey()==EKeys::Escape && Modal.IsValid() && Modal->GetVisibility()!=EVisibility::Collapsed)
        { HideModal(); return FReply::Handled(); }
        if(Event.GetKey()==EKeys::Escape && CurrentPage!=ESeniorLobbyPage::Home)
        { OpenPage(ESeniorLobbyPage::Home);return FReply::Handled(); }
        return SCompoundWidget::OnKeyDown(Geometry,Event);
    }

private:
    TWeakObjectPtr<ASeniorLobbyController> Controller;
    TStrongObjectPtr<UTexture2D> CharacterBackdrop;
    FSlateBrush CharacterBackdropBrush;
    FSlateRoundedBoxBrush FootShadow{FLinearColor(0,0,0,.26f),11.f};
    TSharedPtr<SBox> PageContent,Modal;
    TSharedPtr<SEditableTextBox> Address;
    ESeniorLobbyPage CurrentPage=ESeniorLobbyPage::Home;
    int32 SelectedDifficulty = 1;
    bool bBackdropOnly = false;


    ASeniorLobbyPlayerState* LocalState() const { return Controller.IsValid()?Controller->GetPlayerState<ASeniorLobbyPlayerState>():nullptr; }
    UStoryCampaign* GetStory() const { return Controller.IsValid()?Controller->GetGameInstance<UStoryCampaign>():nullptr; }
    ASeniorLobbyGameState* State() const { return Controller.IsValid()?Controller->GetWorld()->GetGameState<ASeniorLobbyGameState>():nullptr; }
    TArray<ASeniorLobbyPlayerState*> Members() const { return State()?State()->GetMembers():TArray<ASeniorLobbyPlayerState*>(); }
    ASeniorLobbyPlayerState* Member(int32 Index) const { const auto List=Members();return List.IsValidIndex(Index)?List[Index]:nullptr; }
    bool Starting() const { return (State() && State()->bStarting) || (GetStory() && GetStory()->bTravelPending); }
    bool CanStart() const
    {
        if(!Controller.IsValid() || !Controller->IsHost() || !State() || Starting()) return false;
        if(State()->CanStart()) return true;
        const auto Party=Members();
        return Party.Num()==1 && Party[0]==LocalState(); // A solo host starts in one click.
    }
    bool HasSave() const { const auto* S=GetStory();return S && S->Progress && !S->Progress->bCompleted && (S->Progress->bHasCheckpoint || S->Progress->Chapter>1); }
    FString StatusText() const
    {
        if(!Controller.IsValid()) return TEXT("Connecting...");
        const FString Message=Controller->GetLobbyMessage();
        if(!Message.IsEmpty()) return Message;
        if(Starting()) return TEXT("Loading the chapter for your party...");
        if(!State() || !LocalState() || Members().IsEmpty()) return TEXT("Preparing the party...");
        if(Members().Num()==1 && Controller->IsHost())
            return TEXT("Choose your character and weapon, then select Start Game.");
        if(!LocalState()->bReady)
            return TEXT("Choose your character and weapon, then select Ready Up.");
        if(Controller->IsHost() && !State()->CanStart())
            return TEXT("Waiting for the other players to ready up.");
        return Controller->IsHost()?TEXT("Your party is ready. Select Start Game.")
            :TEXT("Ready. Waiting for the host to start the story.");
    }
    TSharedRef<SWidget> MakeStandingPlayer(int32 Index)
    {
        using namespace SeniorUI;
        auto Stage=SNew(SConstraintCanvas);
        Place(Stage,88,435,184,22,SNew(SImage).Image(&FootShadow)
            .Visibility_Lambda([this,Index](){const auto* P=Member(Index);return P && P->CharacterIndex!=0?EVisibility::HitTestInvisible:EVisibility::Collapsed;}));
        Place(Stage,-52,0,464,464,SNew(SBox)
            .Visibility_Lambda([this,Index](){return Member(Index)?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
            [MakeSeniorCharacterPreview(Controller.IsValid()?Controller->GetWorld():nullptr,
                TAttribute<int32>::CreateLambda([this,Index](){
                    const auto* P=Member(Index);
                    return CurrentPage==ESeniorLobbyPage::Home && P ? P->CharacterIndex : -1;
                }),false,TAttribute<int32>::CreateLambda([this,Index](){ const auto* P=Member(Index); return P ? P->LoadoutIndex : 0; }))]);
        Place(Stage,55,240,250,140,SNew(SButton).ButtonStyle(FCoreStyle::Get(),"NoBorder").HAlign(HAlign_Center)
            .Visibility_Lambda([this,Index](){return Member(Index)?EVisibility::Collapsed:EVisibility::Visible;})
            .OnClicked_Lambda([this](){ShowNetwork();return FReply::Handled();})
            [SNew(SVerticalBox)
             +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Label(TEXT("+"),52,White)]
             +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Label(TEXT("INVITE PLAYER"),15,White,true)]]);
        Place(Stage,44,462,272,54,SNew(SBorder).Padding(FMargin(12,5)).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor_Lambda([this,Index](){return Member(Index)?FLinearColor(.012f,.014f,.019f,.82f):FLinearColor(.012f,.014f,.019f,.38f);})
            [SNew(SVerticalBox)
             +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [SNew(STextBlock).Font(Font(14,true)).ColorAndOpacity(White).Text_Lambda([this,Index](){
                    auto* P=Member(Index);return FText::FromString(P?SeniorRoster::Label(P->CharacterIndex)+(P==LocalState()?TEXT(" / YOU"):TEXT("")):FString::Printf(TEXT("PLAYER %d"),Index+1));})]
             +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [SNew(STextBlock).Font(Font(12,true)).ColorAndOpacity_Lambda([this,Index](){auto* P=Member(Index);return P && P->bReady?Green:Muted;})
                 .Text_Lambda([this,Index](){auto* P=Member(Index);return FText::FromString(!P?TEXT("OPEN SLOT"):P->bIsHost?(P->bReady?TEXT("HOST / READY"):TEXT("HOST / CHOOSING")):(P->bReady?TEXT("READY"):TEXT("CHOOSING")));})]]);
        return Stage;
    }
    void OpenPage(ESeniorLobbyPage Page)
    {
        if(!PageContent.IsValid())return;
        CurrentPage=Page;
        HideModal();
        TSharedRef<SWidget> FocusTarget=SharedThis(this);
        if(Page==ESeniorLobbyPage::Home)
        {
            PageContent->SetVisibility(EVisibility::Collapsed);
            PageContent->SetContent(SNullWidget::NullWidget);
        }
        else
        {
            FocusTarget=MakeSeniorLobbyPage(Controller.Get(),Page,FSimpleDelegate::CreateSP(this,&SSeniorLobbyWidget::BackToLobby));
            PageContent->SetContent(FocusTarget);
            PageContent->SetVisibility(EVisibility::SelfHitTestInvisible);
        }
        FSlateApplication::Get().SetKeyboardFocus(FocusTarget,EFocusCause::SetDirectly);
    }
    void BackToLobby(){OpenPage(ESeniorLobbyPage::Home);}
    void HideModal() { if(Modal.IsValid()){Modal->SetVisibility(EVisibility::Collapsed);Modal->SetContent(SNullWidget::NullWidget);} }
    void ShowModal(TSharedRef<SWidget> Content,float Width=650.f)
    {
        Modal->SetContent(SNew(SBorder).Padding(20).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(0,0,0,.8f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
             [SNew(SBox).WidthOverride(Width)
              [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(SeniorUI::Panel).Padding(32)[Content]]]]);
        Modal->SetVisibility(EVisibility::Visible);
    }

    TSharedRef<SWidget> DifficultyChoice(int32 Difficulty)
    {
        using namespace SeniorUI;
        static const TCHAR* Labels[]={TEXT("EASY"),TEXT("NORMAL"),TEXT("HARD")};
        static const TCHAR* Tags[]={TEXT("STORY FOCUSED"),TEXT("INTENDED BALANCE"),TEXT("HIGH PRESSURE")};
        return SNew(SButton).ButtonStyle(&LobbyButtonStyle()).ContentPadding(FMargin(18,15))
            .HAlign(HAlign_Center).VAlign(VAlign_Center)
            .ButtonColorAndOpacity_Lambda([this,Difficulty]() {
                return SelectedDifficulty==Difficulty?FLinearColor(.72f,.16f,.19f,1):FLinearColor::White;
            })
            .OnClicked_Lambda([this,Difficulty]() { SelectedDifficulty=Difficulty;return FReply::Handled(); })
            [SNew(SVerticalBox)
             +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Label(Labels[Difficulty],20,White,true)]
             +SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0).HAlign(HAlign_Center)[Label(Tags[Difficulty],10,Muted,true)]];
    }

    void BeginStory(bool bResume)
    {
        if(!Controller.IsValid() || !Controller->IsHost())return;
        if(auto* Story=GetStory()) Story->SetDifficulty(SelectedDifficulty);
        HideModal();
        Controller->StartStory(bResume);
    }

    void ShowStartGameMenu(bool bUseSavedDifficulty=true)
    {
        using namespace SeniorUI;
        if(!CanStart())return;
        if(bUseSavedDifficulty)
            if(const auto* Story=GetStory()) SelectedDifficulty=FMath::Clamp(Story->SelectedDifficulty,0,2);
        auto DifficultyRow=SNew(SHorizontalBox);
        for(int32 I=0;I<3;++I)
            DifficultyRow->AddSlot().FillWidth(1).Padding(I==0?0:6,0,I==2?0:6,0)[DifficultyChoice(I)];
        static const TCHAR* Descriptions[]={
            TEXT("A calmer way to experience the story."),
            TEXT("The intended balance for a first playthrough."),
            TEXT("A more demanding version of the campaign.")};
        auto Rows=SNew(SVerticalBox);
        Rows->AddSlot().AutoHeight()[Label(TEXT("START GAME"),30,White,true)];
        Rows->AddSlot().AutoHeight().Padding(0,4,0,24)[Label(TEXT("Choose a difficulty, then begin a new story or continue your last checkpoint."),15,Muted)];
        Rows->AddSlot().AutoHeight().Padding(0,0,0,9)[Label(TEXT("DIFFICULTY"),12,FLinearColor(.82f,.35f,.35f,1),true)];
        Rows->AddSlot().AutoHeight()[DifficultyRow];
        Rows->AddSlot().AutoHeight().Padding(0,12,0,25)
            [SNew(STextBlock).Font(Font(14)).ColorAndOpacity(Muted).Justification(ETextJustify::Center)
             .Text_Lambda([this]() { return FText::FromString(Descriptions[FMath::Clamp(SelectedDifficulty,0,2)]); })];
        Rows->AddSlot().AutoHeight().Padding(0,0,0,12)
            [SNew(SButton).ButtonStyle(&LobbyButtonStyle()).ContentPadding(FMargin(18,15))
             .HAlign(HAlign_Center).VAlign(VAlign_Center).IsEnabled(HasSave())
             .OnClicked_Lambda([this]() { BeginStory(true);return FReply::Handled(); })
             [SNew(SVerticalBox)
              +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [Label(HasSave()?TEXT("CONTINUE FROM LAST CHECKPOINT"):TEXT("NO CHECKPOINT AVAILABLE"),18,HasSave()?White:Muted,true)]
              +SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0).HAlign(HAlign_Center)
                [Label(HasSave()?FString::Printf(TEXT("CHAPTER %d  /  CHECKPOINT %d"),GetStory()->Progress->Chapter,GetStory()->Progress->Checkpoint):TEXT("Start a new game to create one."),11,Muted,true)]]];
        Rows->AddSlot().AutoHeight().Padding(0,0,0,12)
            [SNew(SButton).ButtonStyle(&LobbyButtonStyle(true)).ContentPadding(FMargin(18,16))
             .HAlign(HAlign_Center).VAlign(VAlign_Center)
             .OnClicked_Lambda([this]() { if(HasSave())ShowNewStoryConfirmation();else BeginStory(false);return FReply::Handled(); })
             [Label(TEXT("START NEW GAME"),19,White,true)]];
        Rows->AddSlot().AutoHeight()[Button(TEXT("BACK"),[this](){HideModal();})];
        ShowModal(Rows,850.f);
    }
    void ShowNetwork()
    {
        using namespace SeniorUI;
        auto Rows=SNew(SVerticalBox);
        Rows->AddSlot().AutoHeight().Padding(0,0,0,12)[Label(TEXT("PLAY TOGETHER"),28,White,true)];
        Rows->AddSlot().AutoHeight().Padding(0,0,0,20)[Label(TEXT("Up to 3 players on the same local network."),16,Muted)];
        Rows->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("HOST A LAN LOBBY"),[this](){if(Controller.IsValid())Controller->HostLAN();HideModal();},true)];
        Rows->AddSlot().AutoHeight().Padding(0,0,0,20)[SNew(STextBlock).Font(Font(15)).ColorAndOpacity(White).Text_Lambda([this](){return FText::FromString(Controller.IsValid()?TEXT("This computer: ")+Controller->GetHostAddress():TEXT(""));})];
        Rows->AddSlot().AutoHeight().Padding(0,0,0,8)[Label(TEXT("JOIN A FRIEND"),17,White,true)];
        Rows->AddSlot().AutoHeight().Padding(0,0,0,12)[SAssignNew(Address,SEditableTextBox).Font(Font(19)).Padding(FMargin(12))
            .HintText(FText::FromString(TEXT("Host address, e.g. 192.168.1.10")))
            .OnTextCommitted_Lambda([this](const FText&,ETextCommit::Type Type){if(Type==ETextCommit::OnEnter)JoinAddress();})];
        Rows->AddSlot().AutoHeight().Padding(0,0,0,14)[Button(TEXT("JOIN LOBBY"),[this](){JoinAddress();})];
        Rows->AddSlot().AutoHeight().Padding(0,0,0,14)[SNew(STextBlock).Font(Font(14)).ColorAndOpacity(Orange).WrapTextAt(560)
            .Text_Lambda([this](){return FText::FromString(Controller.IsValid()?Controller->GetLobbyMessage():TEXT(""));})];
        Rows->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("LEAVE PARTY / PLAY SOLO"),[this](){if(Controller.IsValid())Controller->LeaveLobby();HideModal();})];
        Rows->AddSlot().AutoHeight()[Button(TEXT("BACK"),[this](){HideModal();})];
        ShowModal(Rows);
    }
    void JoinAddress()
    {
        if(Controller.IsValid() && Address.IsValid())
        {
            const FString IP=Address->GetText().ToString().TrimStartAndEnd();
            Controller->JoinLAN(IP);
            if(ASeniorLobbyController::IsValidLANAddress(IP)) HideModal();
        }
    }
    void ShowNewStoryConfirmation()
    {
        using namespace SeniorUI;
        ShowModal(SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)[Label(TEXT("START A NEW STORY?"),26,White,true)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,24)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("This replaces your saved chapter and checkpoint and starts on %s difficulty."),*UStoryCampaign::DifficultyName(SelectedDifficulty)))).Font(Font(17)).ColorAndOpacity(White).WrapTextAt(560)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("START NEW STORY"),[this](){BeginStory(false);},true)]
            +SVerticalBox::Slot().AutoHeight()[Button(TEXT("KEEP SAVE / BACK"),[this](){ShowStartGameMenu(false);})]);
    }
};

class SSeniorLoadingWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSeniorLoadingWidget){} SLATE_ARGUMENT(int32,Chapter) SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        using namespace SeniorUI;
        Texture.Reset(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Lobby/UI/T_LobbyBackdrop.T_LobbyBackdrop")));
        InitBrush(Brush,Texture.Get());
        const int32 Chapter=FMath::Clamp(Args._Chapter,1,3);
        const TCHAR* Titles[]={TEXT("DEFEND\nTHE HOUSE"),TEXT("CROSS\nTHE CAMPUS"),TEXT("THE\nFORT HOUSE")};
        const TCHAR* Goals[]={TEXT("Survive the seniors and keep the house standing."),TEXT("Gather what you need to enter the Fort House."),TEXT("Face the King Senior. Finish this together.")};
        auto Canvas=SNew(SConstraintCanvas);
        Place(Canvas,55,40,750,60,Label(TEXT("SENIOR SENDOFF"),36,White,true));
        Place(Canvas,55,210,600,38,Label(FString::Printf(TEXT("CHAPTER %d"),Chapter),22,Orange));
        Place(Canvas,55,270,780,230,Label(Titles[Chapter-1],68,White,true));
        Place(Canvas,58,515,720,85,SNew(STextBlock).Text(FText::FromString(Goals[Chapter-1])).Font(Font(23)).ColorAndOpacity(White).WrapTextAt(640));
        const TCHAR* Tips[]={TEXT("TIP   Use prep time to move furniture and block entrances."),TEXT("TIP   Stay together as you make your way across campus."),TEXT("TIP   Watch your teammates' backs during the final fight.")};
        Place(Canvas,58,760,1350,40,Label(Tips[Chapter-1],20,White));
        Place(Canvas,58,825,1240,5,SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Orange));
        Place(Canvas,58,850,1150,30,Label(TEXT("LOADING YOUR STORY"),17,Muted));
        Place(Canvas,1360,820,150,40,SNew(SThrobber).NumPieces(3));
        ChildSlot[SNew(SOverlay)
            +SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFill)[SNew(SImage).Image(&Brush)]]
            +SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0,0,0,.68f))]
            +SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1600).HeightOverride(900)[Canvas]]]];
    }
private:
    TStrongObjectPtr<UTexture2D> Texture;
    FSlateBrush Brush;
};

TSharedRef<SWidget> MakeSeniorLobbyWidget(ASeniorLobbyController* Controller)
{
    return SNew(SSeniorLobbyWidget).Controller(Controller);
}
TSharedRef<SWidget> MakeSeniorLoadingWidget(int32 Chapter)
{
    return SNew(SSeniorLoadingWidget).Chapter(Chapter);
}
