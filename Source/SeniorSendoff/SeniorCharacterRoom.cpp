#include "SeniorCharacterRoom.h"
#include "SeniorCharacterProfiles.h"
#include "SeniorCharacterRoster.h"
#include "SeniorWeaponPresentation.h"
#include "SeniorSelectionRoom.h"
#include "Components/PointLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"

namespace {
constexpr float ArrowPlateWidth=40.f;
constexpr float WeaponShelfTop[2]={148.7f,80.7f};
constexpr float WeaponStandRise=11.f;
TSharedRef<STextBlock> Ink(const FString& Value,int32 Size,FLinearColor Color=FLinearColor(.022,.055,.065,1),bool Bold=false,float WrapAt=744.f)
{
    return SNew(STextBlock).Text(FText::FromString(Value)).Font(FCoreStyle::GetDefaultFontStyle(Bold?"Bold":"Regular",Size)).ColorAndOpacity(Color).WrapTextAt(WrapAt);
}
}
FVector FSeniorCharacterRoom::RosterLampPosition(int32 Index)
{
    const float Step=SeniorRoster::Count>1?130.f/float(SeniorRoster::Count-1):0.f;
    return FVector(106.4f,245.f-Index*Step,10.5f);
}
void FSeniorCharacterRoom::Build(AActor* InOwner)
{
    Owner=InOwner;
    auto* R=NewObject<USceneComponent>(InOwner,TEXT("PhysicalCharacterRoom")); InOwner->AddInstanceComponent(R);
    R->SetupAttachment(InOwner->GetRootComponent());R->RegisterComponent();Root=R;
    auto Mesh=[&](const TCHAR* Shape,FVector Position,FVector Size,const TCHAR* Material,FRotator Rotation=FRotator::ZeroRotator,
        USceneComponent* Parent=nullptr,UMaterialInterface* MaterialOverride=nullptr)
    {
        auto* C=NewObject<UStaticMeshComponent>(InOwner);InOwner->AddInstanceComponent(C);C->SetupAttachment(Parent?Parent:R);
        C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"),Shape,Shape)));
        if(FCString::Strcmp(Shape,TEXT("Cube"))==0)
            if(auto* Soft=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Lobby/CharacterRoom/SM_RoomBeveledCube.SM_RoomBeveledCube")))C->SetStaticMesh(Soft);
        C->SetMaterial(0,MaterialOverride?MaterialOverride:
            LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Lobby/CharacterRoom/%s.%s"),Material,Material)));
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
    for(float Y:{163.f,337.f})
    {
        Mesh(TEXT("Cube"),FVector(-67,Y,147),FVector(14,7,202),TEXT("M_BlackSteel"));
        for(float Z:{54.f,239.f})Mesh(TEXT("Cylinder"),FVector(-58,Y,Z),FVector(3,3,2),TEXT("M_Aluminum"),FRotator(90,0,0));
    }
    Mesh(TEXT("Cube"),FVector(-60,235,143),FVector(6,226,210),TEXT("M_Aluminum"));
    Mesh(TEXT("Cube"),FVector(-56.7,235,143),FVector(.7,217,201),TEXT("M_Whiteboard"));
    Mesh(TEXT("Cube"),FVector(-48,235,37),FVector(25,223,3),TEXT("M_Aluminum"));
    for(int32 I=0;I<3;++I)Mesh(TEXT("Cylinder"),FVector(-42,201+I*14,40),FVector(1.8,1.8,12),I==1?TEXT("M_Wood"):TEXT("M_BlackSteel"),FRotator(0,0,90));
    Mesh(TEXT("Cube"),FVector(-44,294,41),FVector(8,21,5),TEXT("M_BlackSteel"));

    // A worn library case repurposed as an armory: a stepped crown, thinner
    // shelves, inset display backs and hardware give the equipment real depth.
    for(float Y:{-301.f,-129.f})
    {
        Mesh(TEXT("Cube"),FVector(-35,Y,137),FVector(76,5,264),TEXT("M_Wood"));
        Mesh(TEXT("Cube"),FVector(3.9f,Y,137),FVector(2.2f,5.7f,262),TEXT("M_BlackSteel"));
        Mesh(TEXT("Cube"),FVector(5.3f,Y,137),FVector(.8f,2.2f,257),TEXT("M_Aluminum"));
    }
    Mesh(TEXT("Cube"),FVector(-72,-215,140),FVector(4,170,256),TEXT("M_Wood"));
    for(float Z:{112.f,180.f})
    {
        Mesh(TEXT("Cube"),FVector(-69.5f,-215,Z),FVector(1.5f,154,58),TEXT("M_Wood"));
        for(float Y:{-285.f,-145.f})
            Mesh(TEXT("Cube"),FVector(-68.3f,Y,Z),FVector(.8f,1.5f,55),TEXT("M_BlackSteel"));
    }
    for(float Z:{10.f,78.f,146.f,214.f,270.f})
    {
        Mesh(TEXT("Cube"),FVector(-34,-215,Z),FVector(78,176,5),TEXT("M_Wood"));
        Mesh(TEXT("Cube"),FVector(4.5f,-215,Z),FVector(2.5f,175,5.5f),TEXT("M_BlackSteel"));
        Mesh(TEXT("Cube"),FVector(6.f,-215,Z+1.4f),FVector(.7f,168,1.1f),TEXT("M_Aluminum"));
    }
    Mesh(TEXT("Cube"),FVector(-35,-215,275.5f),FVector(83,184,5),TEXT("M_Wood"));
    Mesh(TEXT("Cube"),FVector(-35,-215,279),FVector(87,188,2),TEXT("M_BlackSteel"));
    Mesh(TEXT("Cube"),FVector(-30,-215,5),FVector(70,169,10),TEXT("M_BlackSteel"));
    for(float Y:{-291.f,-139.f})for(float X:{-62.f,-4.f})
        Mesh(TEXT("Cube"),FVector(X,Y,3.5f),FVector(6,6,7),TEXT("M_BlackSteel"));
    // Raised wood display stands sit on each shelf. The weapon bounds are
    // lowered onto their felt tops below, so neither prop hangs in midair.
    for(int32 I=0;I<2;++I)
    {
        const float Floor=WeaponShelfTop[I];
        for(float X:{-59.f,-9.f})for(float Y:{-277.f,-153.f})
            Mesh(TEXT("Cube"),FVector(X,Y,Floor+1.15f),FVector(4,4,2.3f),TEXT("M_BlackSteel"));
        Mesh(TEXT("Cube"),FVector(-34,-215,Floor+4.15f),FVector(62,142,3.7f),TEXT("M_Wood"));
        Mesh(TEXT("Cube"),FVector(-2.5f,-215,Floor+4.15f),FVector(.9f,142,3.7f),TEXT("M_BlackSteel"));
        Mesh(TEXT("Cube"),FVector(-34,-215,Floor+5.75f),FVector(59,136,.5f),TEXT("M_Grout"));
        WeaponStandDecks[I]=Mesh(TEXT("Cube"),FVector(-34,-215,Floor+10.25f),FVector(50,80,1.5f),TEXT("M_BlackSteel"));
        for(int32 Support=0;Support<2;++Support)
        {
            const float Y=Support==0?-238.f:-192.f;
            WeaponStandPosts[I][Support]=Mesh(TEXT("Cube"),FVector(-34,Y,Floor+8.f),FVector(3,3,4.5f),TEXT("M_Aluminum"));
        }
    }
    for(float Z:{75.f,143.f})
    {
        Mesh(TEXT("Cube"),FVector(6.3f,-215,Z),FVector(1.5f,153,15),TEXT("M_Aluminum"));
        Mesh(TEXT("Cube"),FVector(7.2f,-215,Z),FVector(.8f,149,12),TEXT("M_BlackSteel"));
        for(float Y:{-286.f,-144.f})
            Mesh(TEXT("Cylinder"),FVector(7.8f,Y,Z),FVector(1.2f,1.2f,1),TEXT("M_Aluminum"),FRotator(90,0,0));
    }
    // The lower shelf is a small, used collection rather than identical blocks.
    // Each hardcover has a page block, separate boards and a detailed spine;
    // the whole book leans from its foot so its parts stay together.
    UMaterialInterface* BookBase=LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    auto BookMaterial=[&](const FLinearColor& Color)->UMaterialInterface*
    {
        if(!BookBase)return LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Lobby/CharacterRoom/M_Wood.M_Wood"));
        auto* Instance=UMaterialInstanceDynamic::Create(BookBase,InOwner);
        Instance->SetVectorParameterValue(TEXT("Color"),Color);
        Instance->SetScalarParameterValue(TEXT("Roughness"),.86f);
        return Instance;
    };
    const FLinearColor CoverColors[]={
        FLinearColor(.22f,.065f,.065f),FLinearColor(.075f,.14f,.09f),
        FLinearColor(.075f,.105f,.18f),FLinearColor(.27f,.19f,.09f),
        FLinearColor(.20f,.11f,.08f),FLinearColor(.07f,.14f,.145f),
        FLinearColor(.15f,.08f,.12f),FLinearColor(.10f,.12f,.13f)};
    UMaterialInterface* Covers[UE_ARRAY_COUNT(CoverColors)];
    for(int32 I=0;I<UE_ARRAY_COUNT(CoverColors);++I)Covers[I]=BookMaterial(CoverColors[I]);
    UMaterialInterface* Pages=BookMaterial(FLinearColor(.67f,.60f,.47f));
    UMaterialInterface* Gold=BookMaterial(FLinearColor(.48f,.33f,.17f));
    UMaterialInterface* PaperLabel=BookMaterial(FLinearColor(.72f,.63f,.43f));
    UMaterialInterface* InkMaterial=BookMaterial(FLinearColor(.055f,.045f,.035f));
    auto BookPiece=[&](USceneComponent* Book,FVector Position,FVector Size,UMaterialInterface* Finish)
    {
        return Mesh(TEXT("Cube"),Position,Size,TEXT("M_Wood"),FRotator::ZeroRotator,Book,Finish);
    };
    auto BookRoot=[&](FVector Position,FRotator Rotation)
    {
        auto* Book=NewObject<USceneComponent>(InOwner);InOwner->AddInstanceComponent(Book);
        Book->SetupAttachment(R);Book->SetRelativeLocation(Position);Book->SetRelativeRotation(Rotation);
        Book->RegisterComponent();return Book;
    };
    float NextBook=-285.f;
    for(int32 I=0;I<12;++I)
    {
        const float Width=5.7f+(I*5%6)*.55f,Height=33.f+(I*7%15),Depth=34.f+(I%3)*1.5f;
        const float Y=NextBook+Width*.5f,Lean=I==3?5.f:(I==7?-4.f:(I==11?3.f:0.f));
        auto* Book=BookRoot(FVector(-31,Y,12.7f),FRotator(0,0,Lean));
        UMaterialInterface* Cover=Covers[(I*3+I/4)%UE_ARRAY_COUNT(Covers)];
        BookPiece(Book,FVector(0,0,Height*.5f),FVector(Depth-2.f,Width-1.1f,Height-1.6f),Pages);
        for(float Side:{-1.f,1.f})
            BookPiece(Book,FVector(0,Side*(Width*.5f-.45f),Height*.5f),FVector(Depth,.9f,Height),Cover);
        for(float Top:{.45f,Height-.45f})
            BookPiece(Book,FVector(0,0,Top),FVector(Depth,Width,.9f),Cover);
        BookPiece(Book,FVector(Depth*.5f-.55f,0,Height*.5f),FVector(1.8f,Width,Height),Cover);
        for(float Band:{.12f,.18f,.79f,.86f})
            BookPiece(Book,FVector(Depth*.5f+.42f,0,Height*Band),FVector(.26f,Width-.8f,.55f),Gold);
        if(I%3==0 || I%3==2)
        {
            BookPiece(Book,FVector(Depth*.5f+.44f,0,Height*.57f),FVector(.22f,Width-1.7f,7.f),PaperLabel);
            for(float Line:{.53f,.59f,.63f})
                BookPiece(Book,FVector(Depth*.5f+.59f,0,Height*Line),FVector(.15f,Width-2.6f,.34f),InkMaterial);
        }
        else for(float Line:{.47f,.52f,.57f})
            BookPiece(Book,FVector(Depth*.5f+.43f,0,Height*Line),FVector(.23f,Width-1.9f,.55f),Gold);
        NextBook+=Width+1.25f;
    }
    // A few flat reference volumes break the silhouette and show their page
    // edges from the side instead of filling every inch with upright spines.
    float StackBottom=12.7f;
    for(int32 I=0;I<3;++I)
    {
        const float Height=3.2f+I*.35f,Length=34.f-I*2.f,Depth=33.f+I;
        auto* Book=BookRoot(FVector(-31,-158.f+(I%2?1.5f:-1.f),StackBottom),FRotator(0,(I-1)*2.f,0));
        UMaterialInterface* Cover=Covers[(I*2+1)%UE_ARRAY_COUNT(Covers)];
        BookPiece(Book,FVector(0,0,Height*.5f),FVector(Depth-2.f,Length-1.6f,Height-1.f),Pages);
        for(float Top:{.25f,Height-.25f})
            BookPiece(Book,FVector(0,0,Top),FVector(Depth,Length,.5f),Cover);
        BookPiece(Book,FVector(Depth*.5f-.4f,0,Height*.5f),FVector(1.f,Length,Height),Cover);
        for(float Side:{-1.f,1.f})
            BookPiece(Book,FVector(0,Side*(Length*.5f-.25f),Height*.5f),FVector(Depth,.5f,Height),Cover);
        BookPiece(Book,FVector(Depth*.5f+.17f,0,Height*.5f),FVector(.2f,Length*.53f,.42f),Gold);
        StackBottom+=Height+.35f;
    }
    for(float Y:{-292.f,-139.f})
    {
        Mesh(TEXT("Cube"),FVector(-13,Y,35),FVector(3,2.2f,44),TEXT("M_BlackSteel"));
        Mesh(TEXT("Cube"),FVector(-27,Y,13.5f),FVector(30,5,2),TEXT("M_BlackSteel"));
    }
    for(int32 I=0;I<2;++I)
    {
        const float Z=I==0?210.f:142.f;
        Mesh(TEXT("Cube"),FVector(-3,-215,Z),FVector(4,141,2),TEXT("M_BlackSteel"));
        ShelfGlowBars[I][0]=Mesh(TEXT("Cube"),FVector(-2.5f,-215,Z-.9f),FVector(1.2f,132,.9f),TEXT("M_BlackSteel"));
        ShelfGlowBars[I][1]=Mesh(TEXT("Cube"),FVector(7.9f,-215,Z-59.2f),FVector(.7f,145,1.3f),TEXT("M_BlackSteel"));
    }
    // The ability lives here, not on the whiteboard. Bring the plaque forward,
    // fill the top cubby and give it a slim illuminated header rail.
    Mesh(TEXT("Cube"),FVector(-13,-215,241),FVector(5,162,48),TEXT("M_BlackSteel"));
    for(float Y:{-296.f,-134.f})Mesh(TEXT("Cube"),FVector(-9.8f,Y,241),FVector(1.5f,2,48),TEXT("M_Aluminum"));
    for(float Z:{217.f,265.f})Mesh(TEXT("Cube"),FVector(-9.8f,-215,Z),FVector(1.5f,162,1.8f),TEXT("M_Aluminum"));
    Mesh(TEXT("Cube"),FVector(-8.8f,-215,263.5f),FVector(.7f,153,1.2f),TEXT("M_Neon"));
    for(float Y:{-277.f,-153.f})
    {
        Mesh(TEXT("Cylinder"),FVector(-13,Y,266.5f),FVector(1.2f,1.2f,5),TEXT("M_Aluminum"));
        Mesh(TEXT("Cylinder"),FVector(-13,Y,269),FVector(4,4,1),TEXT("M_BlackSteel"));
        Mesh(TEXT("Cylinder"),FVector(-8.7f,Y,255),FVector(2,2,2),TEXT("M_Aluminum"),FRotator(90,0,0));
    }
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

    // The character selectors are physical wall controls, not flat buttons
    // pasted over the turntable. Their chevrons are raised metal light bars.
    for(int32 Arrow=0;Arrow<2;++Arrow)
    {
        const FVector ArrowCenter=ArrowPosition(Arrow);
        const float Y=ArrowCenter.Y, Z=ArrowCenter.Z;
        Mesh(TEXT("Cube"),FVector(-58,Y,Z),FVector(6,ArrowPlateWidth,57),TEXT("M_BlackSteel"));
        Mesh(TEXT("Cube"),FVector(-54.8f,Y,Z),FVector(.7f,36,53),TEXT("M_Aluminum"));
        Mesh(TEXT("Cube"),FVector(-54.3f,Y,Z),FVector(.5f,33,50),TEXT("M_BlackSteel"));
        for(float SideY:{-15.f,15.f})for(float SideZ:{-23.f,23.f})
            Mesh(TEXT("Cylinder"),FVector(-53.6f,Y+SideY,Z+SideZ),FVector(1.3f,1.3f,1.1f),TEXT("M_Aluminum"),FRotator(90,0,0));
        for(int32 Stroke=0;Stroke<2;++Stroke)
        {
            const float Roll=(Arrow==0?-1.f:1.f)*(Stroke==0?45.f:-45.f);
            ArrowBars[Arrow][Stroke]=Mesh(TEXT("Cube"),FVector(-53.3f,Y,Z+(Stroke==0?6.f:-6.f)),
                FVector(1.4f,3.3f,20.f),TEXT("M_Aluminum"),FRotator(0,0,Roll));
        }
    }

    // A grounded equipment case carries the roster readout. Its face, raised
    // frame, rivets and inset selection lamps belong to the room capture.
    Mesh(TEXT("Cube"),FVector(90,180,21),FVector(28,160,42),TEXT("M_Concrete"));
    Mesh(TEXT("Cube"),FVector(90,180,41),FVector(28,157,2),TEXT("M_WornFloor"));
    Mesh(TEXT("Cube"),FVector(104.4f,180,21),FVector(.8f,156,38),TEXT("M_Aluminum"));
    Mesh(TEXT("Cube"),FVector(105.f,180,21),FVector(.7f,151,34),TEXT("M_BlackSteel"));
    for(float Z:{3.5f,38.5f})
        Mesh(TEXT("Cube"),FVector(105.6f,180,Z),FVector(1.2f,156,1.5f),TEXT("M_BlackSteel"));
    for(float Y:{108.f,252.f})
    {
        Mesh(TEXT("Cube"),FVector(94,Y,1.5f),FVector(24,8,3),TEXT("M_Aluminum"));
        for(float Z:{6.f,36.f})
            Mesh(TEXT("Cylinder"),FVector(106,Y,Z),FVector(1.3f,1.3f,1),TEXT("M_Aluminum"),FRotator(90,0,0));
    }
    RosterLights.SetNum(SeniorRoster::Count);
    for(int32 I=0;I<SeniorRoster::Count;++I)
    {
        const FVector Lamp=RosterLampPosition(I);
        Mesh(TEXT("Cube"),Lamp-FVector(1.1f,0,0),FVector(1.1f,13,5),TEXT("M_Grout"));
        RosterLights[I]=Mesh(TEXT("Cube"),Lamp,FVector(1.1f,10.5f,3.5f),TEXT("M_Aluminum"));
    }

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
    ReturnGlow=Light(FVector(-36,248,272),FLinearColor(1,.035,.008),2.8,175,14);
    Light(FVector(-5,-215,261),FLinearColor(1,.66,.3),1.5,190,8);
    for(int32 I=0;I<2;++I)
    {
        UPointLightComponent* ShelfLight=Light(FVector(-28,-215,I==0?181.f:113.f),FLinearColor(1,.84,.62),.15f,145,24);
        ShelfLight->SetCastShadows(false);ShelfLights[I]=ShelfLight;
    }

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
    Writing=Surface(TEXT("WorldWhiteboardWriting"),FVector(-56.1,235,143),FVector2D(920,850),.23,TEXT("M_WorldWriting"));
    for(int32 I=0;I<2;++I)Labels[I]=Surface(*FString::Printf(TEXT("WorldShelfLabel%d"),I),FVector(8.1,-215,I==0?143:75),FVector2D(800,72),.175,TEXT("M_WorldWriting"));
    AbilityDisplay=Surface(TEXT("WorldAbilityDisplay"),FVector(-9.8,-215,241),FVector2D(850,250),.18,TEXT("M_WorldWriting"));
    Neon=Surface(TEXT("WorldNeonReturn"),FVector(-57,248,272),FVector2D(960,128),.176,TEXT("M_WorldNeon"));
    RosterCard=Surface(TEXT("WorldRosterCard"),FVector(105.7f,180,27.3f),FVector2D(760,110),.185,TEXT("M_WorldWriting"));
    SetReturnHovered(false);
}

void FSeniorCharacterRoom::SetReturnHovered(bool bHovered)
{
    if(!Neon.IsValid() || (bReturnHoverInitialized && bReturnHovered==bHovered))return;
    bReturnHovered=bHovered;bReturnHoverInitialized=true;
    const FLinearColor LetterColor=bHovered?FLinearColor(1.f,.95f,.9f,1.f):FLinearColor(1.f,.16f,.055f,1.f);
    if(ReturnGlow.IsValid())ReturnGlow->SetLightColor(bHovered?FLinearColor(1.f,.95f,.9f):FLinearColor(1.f,.035f,.008f));
    Neon->SetSlateWidget(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
        [Ink(TEXT("‹ BACK TO LOBBY"),62,LetterColor,true)]);
    Neon->RequestRedraw();
}

void FSeniorCharacterRoom::SetArrowHovered(int32 ArrowIndex)
{
    if(HoveredArrow==ArrowIndex)return;
    HoveredArrow=ArrowIndex;
    UMaterialInterface* Metal=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Lobby/CharacterRoom/M_Aluminum.M_Aluminum"));
    UMaterialInterface* Lit=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Lobby/CharacterRoom/M_Tube.M_Tube"));
    for(int32 Arrow=0;Arrow<2;++Arrow)for(int32 Stroke=0;Stroke<2;++Stroke)
        if(ArrowBars[Arrow][Stroke].IsValid())ArrowBars[Arrow][Stroke]->SetMaterial(0,Arrow==ArrowIndex?Lit:Metal);
}

void FSeniorCharacterRoom::Update(int32 Character,int32 Weapon,float Seconds)
{
    if(!Owner.IsValid())return;
    if(CeilingLight.IsValid())CeilingLight->SetIntensity(6.f*FSeniorSelectionRoom::Light(Seconds));
    if(ShownCharacter==Character && SelectedWeapon==Weapon)return;
    const bool Changed=ShownCharacter!=Character;ShownCharacter=Character;SelectedWeapon=Weapon;
    const auto& P=SeniorProfiles::Get(Character);
    UMaterialInterface* SelectedRail=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Lobby/CharacterRoom/M_Neon.M_Neon"));
    UMaterialInterface* IdleRail=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Lobby/CharacterRoom/M_BlackSteel.M_BlackSteel"));
    for(int32 I=0;I<2;++I)
    {
        const bool bEquipped=I==Weapon;
        if(ShelfLights[I].IsValid())
        {
            ShelfLights[I]->SetIntensity(bEquipped?6.f:.15f);
            ShelfLights[I]->SetLightColor(bEquipped?FLinearColor(1.f,.66f,.38f):FLinearColor(1.f,.84f,.62f));
        }
        for(int32 Bar=0;Bar<2;++Bar)
            if(ShelfGlowBars[I][Bar].IsValid())ShelfGlowBars[I][Bar]->SetMaterial(0,bEquipped?SelectedRail:IdleRail);
    }
    if(Changed)
    {
        RosterCard->SetSlateWidget(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
            [Ink(FString::Printf(TEXT("%02d / %02d"),Character+1,SeniorRoster::Count),56,FLinearColor(.82,.78,.72,1),true)]);
        RosterCard->RequestRedraw();
        UMaterialInterface* Active=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Lobby/CharacterRoom/M_Neon.M_Neon"));
        UMaterialInterface* Idle=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Lobby/CharacterRoom/M_Aluminum.M_Aluminum"));
        for(int32 I=0;I<RosterLights.Num();++I)
            if(RosterLights[I].IsValid())RosterLights[I]->SetMaterial(0,I==Character?Active:Idle);
    }
    AbilityDisplay->SetSlateWidget(SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(24,9,24,4)
            [Ink(TEXT("SPECIAL ABILITY"),28,FLinearColor(1.f,.29f,.14f,1),true)]
        +SVerticalBox::Slot().AutoHeight().Padding(24,0,24,5)
            [Ink(P.Ability,49,FLinearColor(1.f,.97f,.88f,1),true,790.f)]
        +SVerticalBox::Slot().AutoHeight().Padding(24,0,24,6)
            [Ink(P.AbilityDescription,29,FLinearColor(.92f,.88f,.79f,1),false,790.f)]);
    AbilityDisplay->RequestRedraw();
    auto Board=SNew(SVerticalBox);
    if(Character>1)
        Board->AddSlot().AutoHeight().Padding(0,0,0,10)[Ink(SeniorRoster::Label(Character),23,FLinearColor(.32,.025,.02,1),true)];
    const FString BoardTitle=Character<=1?SeniorRoster::Label(Character):FString(P.Role);
    Board->AddSlot().AutoHeight().Padding(0,0,0,9)[Ink(BoardTitle,52,FLinearColor(.012,.035,.043,1),true)];
    Board->AddSlot().AutoHeight().Padding(0,0,0,Character<=1?12:17)
        [Ink(P.Bio,Character<=1?29:30,FLinearColor(.003,.009,.011,1),false,870.f)];
    Board->AddSlot().AutoHeight().Padding(0,0,0,Character<=1?12:17)
        [Ink(TEXT("FIELD NOTES  /  STATS OUT OF 100"),25,FLinearColor(.25,.018,.016,1),true)];
    for(int32 I=0;I<7;++I)
        Board->AddSlot().AutoHeight().Padding(0,0,0,5)
        [SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Ink(SeniorProfiles::StatNames[I],34,FLinearColor(.003,.009,.011,1),true)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12,0)
            [SNew(SBox).WidthOverride(320).HeightOverride(17)
                [SNew(SProgressBar).Percent(FMath::Clamp(P.Stats[I]/100.f,0.f,1.f))
                    .FillColorAndOpacity(FLinearColor(.035,.16,.17,1))]]
            +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(52).HAlign(HAlign_Right)
                [Ink(FString::FromInt(P.Stats[I]),34,FLinearColor(.003,.009,.011,1),true)]]];
    Writing->SetSlateWidget(SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).Padding(FMargin(20.f,15.f,20.f,10.f))[Board]);Writing->RequestRedraw();
    for(int32 I=0;I<2;++I)
    {
        Labels[I]->SetSlateWidget(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
            [Ink(P.Weapons[I],34,I==Weapon?FLinearColor(1.f,.96f,.84f,1):FLinearColor(.72f,.70f,.64f,1),true)]);
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
            else if(Character==0 && I==1)
            {
                Weapons[I]->SetRelativeLocation(FVector(-34,-215,92));
                Weapons[I]->SetRelativeRotation(FRotator::ZeroRotator);
                Weapons[I]->SetRelativeScale3D(FVector(1.85f));
            }
            else
            {
                Weapons[I]->SetRelativeLocation(FVector(-30,-230,I==0?158:90));
                Weapons[I]->SetRelativeRotation(FRotator(0,90,0));
            }
            // Seat the transformed weapon bounds on a fitted stand deck.
            // The deck and its supports resize for different roster props.
            FBox Bounds(ForceInit);
            TArray<USceneComponent*> Pieces;Weapons[I]->GetChildrenComponents(true,Pieces);
            for(auto* Piece:Pieces)if(auto* Primitive=Cast<UPrimitiveComponent>(Piece))
                Bounds+=Primitive->CalcBounds(Primitive->GetComponentTransform()).GetBox();
            if(Bounds.IsValid)
            {
                const double Top=Root->GetComponentLocation().Z+WeaponShelfTop[I]+WeaponStandRise;
                Weapons[I]->SetRelativeLocation(Weapons[I]->GetRelativeLocation()+FVector(0,0,Top-Bounds.Min.Z));
                const FVector LocalCenter=Root->GetComponentTransform().InverseTransformPosition(Bounds.GetCenter());
                const float Width=FMath::Clamp(float(Bounds.GetSize().X)+12.f,26.f,57.f);
                const float Length=FMath::Clamp(float(Bounds.GetSize().Y)+12.f,36.f,110.f);
                const float X=FMath::Clamp(float(LocalCenter.X),-39.f,-26.f);
                const float Y=FMath::Clamp(float(LocalCenter.Y),-290.f+Length*.5f,-140.f-Length*.5f);
                if(WeaponStandDecks[I].IsValid())
                {
                    WeaponStandDecks[I]->SetRelativeLocation(FVector(X,Y,WeaponShelfTop[I]+WeaponStandRise-.75f));
                    WeaponStandDecks[I]->SetRelativeScale3D(FVector(Width,Length,1.5f)/100.f);
                }
                for(int32 Support=0;Support<2;++Support)
                {
                    const float SupportY=Y+(Support==0?-.28f:.28f)*Length;
                    if(WeaponStandPosts[I][Support].IsValid())
                    {
                        FVector Position=WeaponStandPosts[I][Support]->GetRelativeLocation();
                        Position.X=X;Position.Y=SupportY;WeaponStandPosts[I][Support]->SetRelativeLocation(Position);
                    }
                }
            }
        }
    }
}
void FSeniorCharacterRoom::ReleaseWidgets()
{
    for(auto W:{Writing,Labels[0],Labels[1],Neon,AbilityDisplay,RosterCard})if(W.IsValid())W->SetSlateWidget(nullptr);
}
int32 FSeniorCharacterRoom::Pick(const FVector& Origin,const FVector& Direction)const
{
    if(!Root.IsValid() || FMath::Abs(Direction.X)<.001)return -1;
    const FVector O=Origin-Root->GetComponentLocation();
    auto At=[&](float X){return O+Direction*((X-O.X)/Direction.X);};
    const FVector Sign=At(-57);
    if(Sign.Y>157 && Sign.Y<339 && Sign.Z>257 && Sign.Z<287)return 2;
    const FVector Arrows=At(ArrowPosition(0).X);
    if(FMath::Abs(Arrows.Z-ArrowPosition(0).Z)<29.f)
        for(int32 Arrow=0;Arrow<2;++Arrow)
            if(FMath::Abs(Arrows.Y-ArrowPosition(Arrow).Y)<ArrowPlateWidth*.5f)return Arrow+3;
    const FVector Marker=At(RosterLampPosition(0).X);
    if(FMath::Abs(Marker.Z-RosterLampPosition(0).Z)<5.f)
        for(int32 I=0;I<SeniorRoster::Count;++I)
            if(FMath::Abs(Marker.Y-RosterLampPosition(I).Y)<8.f)return 5+I;
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
    if(!Root.IsValid() || !Writing.IsValid() || !Writing->GetRenderTarget() || !Neon.IsValid() || !Neon->GetRenderTarget() ||
        !RosterCard.IsValid() || !RosterCard->GetRenderTarget() || RosterLights.Num()!=SeniorRoster::Count)return false;
    // Equal spacing around Y=0, with clear wall between each plate and fixture.
    if(!FMath::IsNearlyEqual(ArrowPosition(0).Y,-ArrowPosition(1).Y) ||
        ArrowPosition(0).Y+ArrowPlateWidth*.5f>=130.f ||
        ArrowPosition(1).Y-ArrowPlateWidth*.5f<=-126.5f)return false;
    const FVector Base=Root->GetComponentLocation(),View=Base+FVector(660,0,180);
    const FVector Targets[]={FVector(9,-215,180),FVector(9,-215,106),FVector(-57,248,272),
        ArrowPosition(0),ArrowPosition(1)};
    for(int32 I=0;I<5;++I)if(Pick(View,(Base+Targets[I]-View).GetSafeNormal())!=I)return false;
    for(int32 Arrow=0;Arrow<2;++Arrow)for(int32 Stroke=0;Stroke<2;++Stroke)if(!ArrowBars[Arrow][Stroke].IsValid())return false;
    for(int32 I=0;I<SeniorRoster::Count;++I)
        if(!RosterLights[I].IsValid() || Pick(View,(Base+RosterLampPosition(I)-View).GetSafeNormal())!=I+5)return false;
    for(int32 I=0;I<2;++I)
    {
        if(!Weapons[I].IsValid() || !Labels[I].IsValid() || !Labels[I]->GetRenderTarget())return false;
        if(!WeaponStandDecks[I].IsValid())return false;
        for(int32 Support=0;Support<2;++Support)
            if(!WeaponStandPosts[I][Support].IsValid())return false;
        FBox Bounds(ForceInit);TArray<USceneComponent*> Pieces;Weapons[I]->GetChildrenComponents(true,Pieces);
        for(auto* Piece:Pieces)if(auto* Primitive=Cast<UPrimitiveComponent>(Piece))Bounds+=Primitive->CalcBounds(Primitive->GetComponentTransform()).GetBox();
        if(!Bounds.IsValid || FMath::Abs(Bounds.Min.Z-Base.Z-WeaponShelfTop[I]-WeaponStandRise)>1)return false;
    }
    return Writing->GetWidgetSpace()==EWidgetSpace::World;
}
