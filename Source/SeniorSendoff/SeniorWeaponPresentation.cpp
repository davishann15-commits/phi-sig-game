#include "SeniorWeaponPresentation.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PointLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SCompoundWidget.h"
#include "Rendering/DrawElements.h"

namespace {
class SWeaponWall final:public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWeaponWall){} SLATE_ARGUMENT(UWorld*,World) SLATE_ATTRIBUTE(int32,Character) SLATE_ARGUMENT(int32,Weapon) SLATE_END_ARGS()
    void Construct(const FArguments& A)
    {
        World=A._World; Character=A._Character; Weapon=A._Weapon;
        Brush.DrawAs=ESlateBrushDrawType::Image; Brush.ImageSize=FVector2D(640,280);
        SetVisibility(EVisibility::HitTestInvisible); SetCanTick(true);
        ChildSlot[SNew(SImage).Image(&Brush)];
    }
    ~SWeaponWall() { if(Actor.IsValid()) Actor->Destroy(); Brush.SetResourceObject(nullptr); }
    void Tick(const FGeometry& G,double T,float D) override
    {
        SCompoundWidget::Tick(G,T,D);
        if(!World.IsValid() || World->bIsTearingDown)return;
        if(!Actor.IsValid())Create();
        if(!Actor.IsValid() || !Camera.IsValid())return;
        if(Shown!=Character.Get())
        {
            SeniorWeaponPresentation::Destroy(Model.Get());
            Shown=Character.Get();
            Model=SeniorWeaponPresentation::Build(Actor.Get(),Actor->GetRootComponent(),Shown,Weapon);
            // Camera sees the face of the hat, and a slightly elevated side of long arms.
            if(Shown==0 && Weapon==0) Model->SetRelativeRotation(FRotator(0,0,72));
            else Model->SetRelativeRotation(FRotator(-8,0,0));
            Camera->SetRelativeLocation(FVector(Shown==0 && Weapon==0?0:15,-250,0));
            Camera->ClearShowOnlyComponents(); Camera->ShowOnlyActorComponents(Actor.Get());
            Frames=0;
        }
        // No continuous captures for stationary rack props. A short warm-up allows
        // material/texture streaming and newly registered components to settle.
        if(Frames++<80)Camera->CaptureScene();
    }
    int32 OnPaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& R,FSlateWindowElementList& E,int32 L,const FWidgetStyle& S,bool Enabled)const override
    {
        if(Shadow.IsValid())for(int32 I=0;I<3;++I)
            FSlateDrawElement::MakeBox(E,L,G.ToPaintGeometry(FVector2f(G.GetLocalSize()),FSlateLayoutTransform(FVector2f(3+I,5+I))),&ShadowBrush,ESlateDrawEffect::None,FLinearColor(1,1,1,.65));
        return SCompoundWidget::OnPaint(A,G,R,E,L+1,S,Enabled);
    }
private:
    void Create()
    {
        static int32 Serial=0;
        FActorSpawnParameters P; P.ObjectFlags=RF_Transient;
        P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Actor=World->SpawnActor<AActor>(AActor::StaticClass(),FVector(0,0,-4000000-2000*(++Serial)),FRotator::ZeroRotator,P);
        if(!Actor.IsValid())return;
        auto* Root=NewObject<USceneComponent>(Actor.Get()); Actor->AddInstanceComponent(Root);
        Actor->SetRootComponent(Root);Root->RegisterComponent();Root->SetWorldLocation(FVector(0,0,-4000000-2000*Serial));
        for(int32 I=0;I<2;++I)
        {
            auto* Light=NewObject<UPointLightComponent>(Actor.Get());Actor->AddInstanceComponent(Light);Light->SetupAttachment(Root);
            Light->SetRelativeLocation(I==0?FVector(-25,-90,100):FVector(50,-70,10));
            Light->SetUseInverseSquaredFalloff(false);Light->SetIntensity(I==0?7.f:2.f);Light->SetAttenuationRadius(500);
            Light->SetLightColor(I==0?FLinearColor(1,.86,.64):FLinearColor(.7,.8,1));Light->SetCastShadows(false);Light->RegisterComponent();
        }
        Target.Reset(NewObject<UTextureRenderTarget2D>()); Target->ClearColor=FLinearColor(0,0,0,1);
        Target->RenderTargetFormat=RTF_RGBA16f;Target->bForceLinearGamma=true;Target->InitAutoFormat(960,420);Target->UpdateResourceImmediate(true);
        auto* C=NewObject<USceneCaptureComponent2D>(Actor.Get());Actor->AddInstanceComponent(C);C->SetupAttachment(Root);
        C->SetRelativeLocation(FVector(15,-250,0));C->SetRelativeRotation(FRotator(0,90,0));
        C->ProjectionType=ECameraProjectionMode::Orthographic;C->OrthoWidth=125;C->bAutoCalculateOrthoPlanes=false;
        C->TextureTarget=Target.Get();C->CaptureSource=SCS_SceneColorHDR;
        C->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
        C->bCaptureEveryFrame=false;C->bCaptureOnMovement=false;
        C->ShowFlags.SetAtmosphere(false);C->ShowFlags.SetFog(false);C->ShowFlags.SetSkyLighting(false);
        C->ShowFlags.SetBloom(false);C->ShowFlags.SetEyeAdaptation(false);C->ShowFlags.SetMotionBlur(false);
        C->ShowFlags.SetLumenGlobalIllumination(false);C->ShowFlags.SetLumenReflections(false);
        C->PostProcessSettings.bOverride_AutoExposureMethod=true;C->PostProcessSettings.AutoExposureMethod=AEM_Manual;
        C->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;C->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
        C->RegisterComponent();Camera=C;
        if(auto* M=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Characters/UI/M_CharacterPreview.M_CharacterPreview")))
        {
            Material.Reset(UMaterialInstanceDynamic::Create(M,GetTransientPackage()));
            Material->SetTextureParameterValue(TEXT("PreviewTexture"),Target.Get());Brush.SetResourceObject(Material.Get());
        }
        if(auto* M=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Characters/UI/M_SelectionShadow.M_SelectionShadow")))
        {
            Shadow.Reset(UMaterialInstanceDynamic::Create(M,GetTransientPackage()));
            Shadow->SetTextureParameterValue(TEXT("PreviewTexture"),Target.Get());ShadowBrush.DrawAs=ESlateBrushDrawType::Image;ShadowBrush.SetResourceObject(Shadow.Get());
        }
    }
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AActor> Actor;TWeakObjectPtr<USceneComponent> Model;
    TWeakObjectPtr<USceneCaptureComponent2D> Camera;
    TStrongObjectPtr<UTextureRenderTarget2D> Target;TStrongObjectPtr<UMaterialInstanceDynamic> Material;
    TStrongObjectPtr<UMaterialInstanceDynamic> Shadow;FSlateBrush ShadowBrush;
    TAttribute<int32> Character;int32 Weapon=0,Shown=-1,Frames=0;FSlateBrush Brush;
};
}
TSharedRef<SWidget> MakeSeniorWeaponWallPreview(UWorld* World,TAttribute<int32> Character,int32 Weapon)
{ return SNew(SWeaponWall).World(World).Character(Character).Weapon(Weapon); }
