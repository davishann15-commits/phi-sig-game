#include "SeniorCharacterRoom.h"
#include "SeniorCharacterProfiles.h"
#include "SeniorCharacterRoster.h"
#include "SeniorWeaponPresentation.h"
#include "SeniorSelectionRoom.h"
#include "Components/PointLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"

namespace {
TSharedRef<STextBlock> Ink(const FString& Value,int32 Size,FLinearColor Color=FLinearColor(.022,.055,.065,1),bool Bold=false)
{
    return SNew(STextBlock).Text(FText::FromString(Value)).Font(FCoreStyle::GetDefaultFontStyle(Bold?"Bold":"Regular",Size)).ColorAndOpacity(Color).WrapTextAt(744);
}
}
void FSeniorCharacterRoom::Build(AActor* InOwner)
{
    Owner=InOwner;
    auto* R=NewObject<USceneComponent>(InOwner,TEXT("PhysicalCharacterRoom")); InOwner->AddInstanceComponent(R);
    R->SetupAttachment(InOwner->GetRootComponent());R->RegisterComponent();Root=R;
    auto Mesh=[&](const TCHAR* Shape,FVector Position,FVector Size,const TCHAR* Material,FRotator Rotation=FRotator::ZeroRotator)
    {
        auto* C=NewObject<UStaticMeshComponent>(InOwner);InOwner->AddInstanceComponent(C);C->SetupAttachment(R);
        C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"),Shape,Shape)));
        if(FCString::Strcmp(Shape,TEXT("Cube"))==0)
            if(auto* Soft=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Lobby/CharacterRoom/SM_RoomBeveledCube.SM_RoomBeveledCube")))C->SetStaticMesh(Soft);
        C->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Lobby/CharacterRoom/%s.%s"),Material,Material)));
        C->SetRelativeLocation(Position);C->SetRelativeScale3D(Size/100.f);C->SetRelativeRotation(Rotation);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(true);C->RegisterComponent();return C;
    };
    // Concrete slab, recessed mortar backing and genuinely modelled blockwork.
    Mesh(TEXT("Cube"),FVector(240,0,-8),FVector(720,900,16),TEXT("M_WornFloor"));
    Mesh(TEXT("Cube"),FVector(-91,0,168),FVector(18,900,336),TEXT("M_Grout"));
    auto* Blocks=NewObject<UInstancedStaticMeshComponent>(InOwner);InOwner->AddInstanceComponent(Blocks);Blocks->SetupAttachment(R);
    Blocks->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Lobby/CharacterRoom/SM_RoomBeveledCube.SM_RoomBeveledCube")));
    Blocks->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Lobby/CharacterRoom/M_Block.M_Block")));
    Blocks->SetCollisionEnabled(ECollisionEnabled::NoCollision);Blocks->RegisterComponent();
    for(int32 Row=0;Row<13;++Row)for(int32 Col=0;Col<20;++Col)
    {
        const float Y=-465+Col*48+(Row%2)*24;
        Blocks->AddInstance(FTransform(FQuat::Identity,FVector(-79,Y,13+Row*26),FVector(.08,.47,.25)));
    }
    for(float Side:{-1.f,1.f})
    {
        Mesh(TEXT("Cube"),FVector(140,Side*417,165),FVector(450,18,330),TEXT("M_Block"));
        Mesh(TEXT("Cube"),FVector(150,Side*404,7),FVector(460,8,14),TEXT("M_BlackSteel"));
        Mesh(TEXT("Cube"),FVector(-69,Side*398,165),FVector(24,20,330),TEXT("M_Concrete"));
    }
    Mesh(TEXT("Cube"),FVector(-67,0,7),FVector(10,820,14),TEXT("M_BlackSteel"));
    Mesh(TEXT("Cube"),FVector(140,0,329),FVector(480,850,12),TEXT("M_BlackSteel"));
    for(float Y:{-300.f,-100.f,100.f,300.f})
        Mesh(TEXT("Cube"),FVector(140,Y,313),FVector(450,9,22),TEXT("M_BlackSteel"));
    // Floor expansion joints are recessed dark strips, not screen-space lines.
    for(float Y:{-300.f,0.f,300.f})Mesh(TEXT("Cube"),FVector(200,Y,.02),FVector(650,.7,.04),TEXT("M_Grout"));
    for(float X:{80.f,310.f,540.f})Mesh(TEXT("Cube"),FVector(X,0,.03),FVector(.7,820,.04),TEXT("M_Grout"));

    // Whiteboard sits off the wall on metal brackets. Everything has thickness.
    for(float Y:{171.f,325.f})
    {
        Mesh(TEXT("Cube"),FVector(-67,Y,147),FVector(14,7,202),TEXT("M_BlackSteel"));
        for(float Z:{54.f,239.f})Mesh(TEXT("Cylinder"),FVector(-58,Y,Z),FVector(3,3,2),TEXT("M_Aluminum"),FRotator(90,0,0));
    }
    Mesh(TEXT("Cube"),FVector(-60,235,143),FVector(6,210,210),TEXT("M_Aluminum"));
    Mesh(TEXT("Cube"),FVector(-56.7,235,143),FVector(.7,201,201),TEXT("M_Whiteboard"));
    Mesh(TEXT("Cube"),FVector(-48,235,37),FVector(25,207,3),TEXT("M_Aluminum"));
    for(int32 I=0;I<3;++I)Mesh(TEXT("Cylinder"),FVector(-42,201+I*14,40),FVector(1.8,1.8,12),I==1?TEXT("M_Wood"):TEXT("M_BlackSteel"),FRotator(0,0,90));
    Mesh(TEXT("Cube"),FVector(-44,294,41),FVector(8,21,5),TEXT("M_BlackSteel"));

    // Four-tier freestanding bookcase, arranged top to bottom.
    for(float Z:{10.f,78.f,146.f,214.f,270.f})
    {
        Mesh(TEXT("Cube"),FVector(-34,-215,Z),FVector(78,176,5),TEXT("M_Wood"));
        Mesh(TEXT("Cube"),FVector(5,-215,Z),FVector(2,176,7),TEXT("M_Wood"));
    }
    for(float Y:{-301.f,-129.f})
        Mesh(TEXT("Cube"),FVector(-35,Y,136),FVector(76,5,272),TEXT("M_Wood"));
    Mesh(TEXT("Cube"),FVector(-72,-215,140),FVector(4,170,256),TEXT("M_Wood"));
    Mesh(TEXT("Cube"),FVector(-30,-215,5),FVector(70,169,10),TEXT("M_BlackSteel"));
    // Individual hardbacks: visible page blocks, covers and spine bands.
    for(int32 I=0;I<19;++I)
    {
        const float Y=-289+I*8.2f,H=31+(I*7)%16,Bottom=216.7f;
        const TCHAR* Cover=I%3==0?TEXT("M_BlackSteel"):TEXT("M_Wood");
        Mesh(TEXT("Cube"),FVector(-29,Y,Bottom+H*.5f),FVector(31,6.4,H),TEXT("M_Whiteboard"));
        for(float Side:{-1.f,1.f})Mesh(TEXT("Cube"),FVector(-29,Y+Side*3.7f,Bottom+H*.5f),FVector(33,.8,H+1),Cover);
        Mesh(TEXT("Cube"),FVector(-12,Y,Bottom+H*.5f),FVector(1.5,8,H+1),Cover);
        for(float Band:{.2f,.8f})Mesh(TEXT("Cube"),FVector(-11.1,Y,Bottom+H*Band),FVector(.3,6.5,.65),TEXT("M_Aluminum"));
    }
    for(float Z:{143.f,75.f})Mesh(TEXT("Cube"),FVector(7,-215,Z),FVector(2,148,14),TEXT("M_BlackSteel"));
    // An inset ability exhibit, not a selectable third weapon.
    Mesh(TEXT("Cube"),FVector(-30,-215,16),FVector(48,144,6),TEXT("M_BlackSteel"));
    Mesh(TEXT("Cube"),FVector(-35,-215,46),FVector(4,146,47),TEXT("M_BlackSteel"));
    for(float Y:{-289.f,-141.f})Mesh(TEXT("Cube"),FVector(-32,Y,46),FVector(3,2,49),TEXT("M_Aluminum"));
    // Ceiling fixture with a casing, twin tubes and safety rails.
    Mesh(TEXT("Cube"),FVector(-3,0,293),FVector(28,160,9),TEXT("M_Aluminum"));
    for(float X:{-11.f,6.f})Mesh(TEXT("Cylinder"),FVector(X,0,286),FVector(4,4,149),TEXT("M_Tube"),FRotator(0,0,90));
    for(float Y:{-73.f,73.f})Mesh(TEXT("Cube"),FVector(-2,Y,283),FVector(32,3,3),TEXT("M_BlackSteel"));
    // Service conduit along the masonry connects the real sign transformer.
    Mesh(TEXT("Cylinder"),FVector(-65,375,155),FVector(2.5,2.5,308),TEXT("M_BlackSteel"));
    Mesh(TEXT("Cube"),FVector(-62,352,265),FVector(9,16,23),TEXT("M_BlackSteel"));
    Mesh(TEXT("Cylinder"),FVector(-60,344,267),FVector(2,2,31),TEXT("M_BlackSteel"),FRotator(0,0,90));
    Mesh(TEXT("Cube"),FVector(-62,248,272),FVector(3,181,29),TEXT("M_BlackSteel"));
    for(float Y:{166.f,330.f})for(float Z:{262.f,282.f})
        Mesh(TEXT("Cylinder"),FVector(-58,Y,Z),FVector(2.6,2.6,6),TEXT("M_Aluminum"),FRotator(90,0,0));

    auto Light=[&](FVector Position,FLinearColor Color,float Intensity,float Radius,float Source)
    {
        auto* L=NewObject<UPointLightComponent>(InOwner);InOwner->AddInstanceComponent(L);L->SetupAttachment(R);
        L->SetRelativeLocation(Position);L->SetUseInverseSquaredFalloff(false);L->SetIntensity(Intensity);
        L->SetAttenuationRadius(Radius);L->SetLightColor(Color);L->SetSourceRadius(Source);L->SetSoftSourceRadius(Source);
        L->SetCastShadows(true);L->RegisterComponent();return L;
    };
    CeilingLight=Light(FVector(12,0,270),FLinearColor(1,.86,.65),6,750,18);
    Light(FVector(200,-100,185),FLinearColor(.68,.75,1),.55,850,40)->SetCastShadows(false);
    Light(FVector(180,240,195),FLinearColor(1,.91,.76),.95,850,50)->SetCastShadows(false);
    Light(FVector(-36,248,272),FLinearColor(1,.035,.008),2.8,175,14);
    Light(FVector(-5,-215,261),FLinearColor(1,.66,.3),1.5,190,8);

    auto Surface=[&](FName Name,FVector Position,FVector2D Draw,float Scale,const TCHAR* Material)
    {
        auto* W=NewObject<UWidgetComponent>(InOwner,Name);InOwner->AddInstanceComponent(W);W->SetupAttachment(R);
        W->SetWidgetSpace(EWidgetSpace::World);W->SetDrawSize(Draw);W->SetPivot(FVector2D(.5,.5));
        W->SetRelativeLocation(Position);W->SetRelativeScale3D(FVector(Scale));W->SetTwoSided(false);
        W->SetBlendMode(EWidgetBlendMode::Masked);W->SetBackgroundColor(FLinearColor::Transparent);
        W->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Lobby/CharacterRoom/%s.%s"),Material,Material)));
        W->SetCollisionEnabled(ECollisionEnabled::NoCollision);W->SetCastShadow(false);
        W->SetTickWhenOffscreen(true);W->SetManuallyRedraw(true);W->RegisterComponent();return W;
    };
    Writing=Surface(TEXT("WorldWhiteboardWriting"),FVector(-56.1,235,143),FVector2D(800,850),.23,TEXT("M_WorldWriting"));
    for(int32 I=0;I<2;++I)Labels[I]=Surface(*FString::Printf(TEXT("WorldShelfLabel%d"),I),FVector(8.1,-215,I==0?143:75),FVector2D(800,72),.175,TEXT("M_WorldWriting"));
    AbilityDisplay=Surface(TEXT("WorldAbilityDisplay"),FVector(-32.8,-215,39),FVector2D(800,240),.17,TEXT("M_WorldWriting"));
    Neon=Surface(TEXT("WorldNeonReturn"),FVector(-57,248,272),FVector2D(960,128),.176,TEXT("M_WorldNeon"));
    Neon->SetSlateWidget(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
        [Ink(TEXT("‹ BACK TO LOBBY"),62,FLinearColor(1,.16,.055,1),true)]);
    Neon->RequestRedraw();
}

void FSeniorCharacterRoom::Update(int32 Character,int32 Weapon,float Seconds)
{
    if(!Owner.IsValid())return;
    if(CeilingLight.IsValid())CeilingLight->SetIntensity(6.f*FSeniorSelectionRoom::Light(Seconds));
    if(ShownCharacter==Character && SelectedWeapon==Weapon)return;
    const bool Changed=ShownCharacter!=Character;ShownCharacter=Character;SelectedWeapon=Weapon;
    const auto& P=SeniorProfiles::Get(Character);
    AbilityDisplay->SetSlateWidget(SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(18,6)[Ink(TEXT("SPECIAL ABILITY"),23,FLinearColor(.65,.53,.30,1),true)]
        +SVerticalBox::Slot().AutoHeight().Padding(18,3)[Ink(P.Ability,36,FLinearColor(.95,.91,.76,1),true)]
        +SVerticalBox::Slot().AutoHeight().Padding(18,6)[Ink(P.AbilityDescription,24,FLinearColor(.85,.82,.73,1))]);
    AbilityDisplay->RequestRedraw();
    auto Board=SNew(SVerticalBox);
    Board->AddSlot().AutoHeight().Padding(0,0,0,10)[Ink(Character==0?TEXT("BRAXTON HUNGATE  /  5′9″  /  155 LB"):SeniorRoster::Label(Character),23,FLinearColor(.32,.025,.02,1),true)];
    Board->AddSlot().AutoHeight().Padding(0,0,0,10)[Ink(P.Role,46,FLinearColor(.018,.05,.06,1),true)];
    Board->AddSlot().AutoHeight().Padding(0,0,0,12)[Ink(P.Bio,26,FLinearColor(.008,.015,.018,1))];
    Board->AddSlot().AutoHeight().Padding(0,0,0,14)[Ink(TEXT("FIELD NOTES  /  STATS OUT OF 100"),21,FLinearColor(.32,.025,.02,1),true)];
    for(int32 I=0;I<7;++I)
        Board->AddSlot().AutoHeight().Padding(0,0,0,3)
        [SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Ink(SeniorProfiles::StatNames[I],28,FLinearColor(.008,.015,.018,1),true)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12,0)
            [SNew(SBox).WidthOverride(280).HeightOverride(12)
                [SNew(SProgressBar).Percent(FMath::Clamp(P.Stats[I]/100.f,0.f,1.f))
                    .FillColorAndOpacity(FLinearColor(.035,.16,.17,1))]]
            +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(52).HAlign(HAlign_Right)
                [Ink(FString::FromInt(P.Stats[I]),28,FLinearColor(.008,.015,.018,1),true)]]];
    Board->AddSlot().AutoHeight().Padding(0,10,0,6)[Ink(FString(TEXT("ABILITY: "))+P.Ability,26,FLinearColor(.32,.025,.02,1),true)];
    Board->AddSlot().AutoHeight()[Ink(P.AbilityDescription,24,FLinearColor(.008,.015,.018,1))];
    Writing->SetSlateWidget(SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).Padding(26)[Board]);Writing->RequestRedraw();
    for(int32 I=0;I<2;++I)
    {
        Labels[I]->SetSlateWidget(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
            [Ink(FString(P.Weapons[I])+(I==Weapon?TEXT("    • IN HAND"):TEXT("    • SELECT")),31,FLinearColor(.85,.80,.65,1),true)]);
        Labels[I]->RequestRedraw();
        if(Changed)
        {
            SeniorWeaponPresentation::Destroy(Weapons[I].Get());
            Weapons[I]=SeniorWeaponPresentation::Build(Owner.Get(),Root.Get(),Character,I);
            if(Character==0 && I==0)
            {
                Weapons[I]->SetRelativeLocation(FVector(-34,-215,178));
                Weapons[I]->SetRelativeRotation(FRotator(78,0,0));
            }
            else
            {
                Weapons[I]->SetRelativeLocation(FVector(-30,-230,I==0?158:90));
                Weapons[I]->SetRelativeRotation(FRotator(0,90,0));
            }
            // Ground the actual transformed mesh bounds on the shelf surface.
            // This also handles differently sized weapons across the roster.
            FBox Bounds(ForceInit);
            TArray<USceneComponent*> Pieces;Weapons[I]->GetChildrenComponents(true,Pieces);
            for(auto* Piece:Pieces)if(auto* Primitive=Cast<UPrimitiveComponent>(Piece))
                Bounds+=Primitive->CalcBounds(Primitive->GetComponentTransform()).GetBox();
            if(Bounds.IsValid)
            {
                const double Top=Root->GetComponentLocation().Z+(I==0?148.7:80.7);
                Weapons[I]->SetRelativeLocation(Weapons[I]->GetRelativeLocation()+FVector(0,0,Top-Bounds.Min.Z));
            }
        }
    }
}
void FSeniorCharacterRoom::ReleaseWidgets()
{
    for(auto W:{Writing,Labels[0],Labels[1],Neon,AbilityDisplay})if(W.IsValid())W->SetSlateWidget(nullptr);
}
int32 FSeniorCharacterRoom::Pick(const FVector& Origin,const FVector& Direction)const
{
    if(!Root.IsValid() || FMath::Abs(Direction.X)<.001)return -1;
    const FVector O=Origin-Root->GetComponentLocation();
    auto At=[&](float X){return O+Direction*((X-O.X)/Direction.X);};
    const FVector Sign=At(-57);
    if(Sign.Y>157 && Sign.Y<339 && Sign.Z>257 && Sign.Z<287)return 2;
    const FVector Shelf=At(9);
    if(Shelf.Y>-303 && Shelf.Y<-127)
    {
        if(Shelf.Z>136 && Shelf.Z<211)return 0;
        if(Shelf.Z>68 && Shelf.Z<136)return 1;
    }
    return -1;
}
bool FSeniorCharacterRoom::Validate()const
{
    if(!Root.IsValid() || !Writing.IsValid() || !Writing->GetRenderTarget() || !Neon.IsValid() || !Neon->GetRenderTarget())return false;
    const FVector Base=Root->GetComponentLocation(),View=Base+FVector(660,0,180);
    const FVector Targets[]={FVector(9,-215,180),FVector(9,-215,106),FVector(-57,248,272)};
    for(int32 I=0;I<3;++I)if(Pick(View,(Base+Targets[I]-View).GetSafeNormal())!=I)return false;
    for(int32 I=0;I<2;++I)
    {
        if(!Weapons[I].IsValid() || !Labels[I].IsValid() || !Labels[I]->GetRenderTarget())return false;
        FBox Bounds(ForceInit);TArray<USceneComponent*> Pieces;Weapons[I]->GetChildrenComponents(true,Pieces);
        for(auto* Piece:Pieces)if(auto* Primitive=Cast<UPrimitiveComponent>(Piece))Bounds+=Primitive->CalcBounds(Primitive->GetComponentTransform()).GetBox();
        if(!Bounds.IsValid || FMath::Abs(Bounds.Min.Z-Base.Z-(I==0?148.7:80.7))>1)return false;
    }
    return Writing->GetWidgetSpace()==EWidgetSpace::World;
}
