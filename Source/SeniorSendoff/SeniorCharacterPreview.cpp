#include "SeniorCharacterPreview.h"
#include "SeniorCharacterRoster.h"
#include "SeniorClothMotion.h"
#include "SeniorBraxtonVisual.h"
#include "SeniorDouliAnim.h"
#include "SeniorDouliIdle.h"
#include "SeniorSelectionRoom.h"
#include "SeniorWeaponPresentation.h"
#include "SeniorCharacterRoom.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraTypes.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SCompoundWidget.h"

namespace
{
// One camera ruler for the whole roster: 216 cm vertically, with every pair of feet at Z=0.
constexpr float FullBodyOrthoWidth = 216.0f;
constexpr float FullBodyAimZ = 98.0f;
constexpr float FaceZoom = 0.22f;

class SSeniorCharacterPreview final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSeniorCharacterPreview) : _World(nullptr), _CharacterIndex(0), _Interactive(true), _Room(false) {}
        SLATE_ARGUMENT(UWorld*, World)
        SLATE_ATTRIBUTE(int32, CharacterIndex)
        SLATE_ATTRIBUTE(int32, WeaponIndex)
        SLATE_ARGUMENT(bool, Interactive)
        SLATE_ARGUMENT(bool, Room)
        SLATE_ARGUMENT(TFunction<void(int32)>, OnWeapon)
        SLATE_EVENT(FSimpleDelegate, OnBack)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        PreviewWorld = Args._World;
        CharacterIndex = Args._CharacterIndex;
        WeaponIndex = Args._WeaponIndex;
        bInteractive = Args._Interactive;
        bRoom=Args._Room; OnWeapon=Args._OnWeapon; OnBack=Args._OnBack;
        Brush.DrawAs = ESlateBrushDrawType::Image;
        Brush.ImageSize = FVector2D(768, 768);
        SetCanTick(true);
        // The image is a changing render target. Repaint also lets hidden pages suspend captures.
        ForceVolatile(true);
        SetVisibility(bInteractive ? EVisibility::Visible : EVisibility::HitTestInvisible);
        if (bInteractive) SetCursor(EMouseCursor::GrabHand);
        ChildSlot[SNew(SImage).Image(&Brush)];
    }

    virtual ~SSeniorCharacterPreview() override
    {
        Room.ReleaseWidgets();
        if (Braxton.IsValid()) Braxton->Destroy();
        if (AActor* Actor = PreviewActor.Get())
        {
            if (UWorld* World = Actor->GetWorld(); World && !World->bIsTearingDown)
            {
                Actor->Destroy();
            }
        }
        Brush.SetResourceObject(nullptr);
    }

    virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override
    {
        SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
        const double Now = FPlatformTime::Seconds();
        const int32 WantedCharacter = CharacterIndex.Get(INDEX_NONE);
        if (!SeniorRoster::IsValidIndex(WantedCharacter)) return;
        if (LastPaintTime <= 0.0 || Now - LastPaintTime > 0.2 || Geometry.GetLocalSize().IsNearlyZero())
        {
            if (Braxton.IsValid()) Braxton->SetVisualActive(false);
            return;
        }
        UWorld* World = PreviewWorld.Get();
        if (!World || World->bIsTearingDown || World->GetNetMode() == NM_DedicatedServer) return;
        if (!PreviewActor.IsValid() && !bCreationFailed) CreateScene(World);
        if (!Capture.IsValid() || !Mesh.IsValid()) return;

        if (WantedCharacter != ShownCharacter) SetCharacter(WantedCharacter);
        const int32 WantedWeapon=FMath::Clamp(WeaponIndex.Get(0),0,1);
        if(bRoom)Room.Update(WantedCharacter,WantedWeapon,FSeniorSelectionRoom::Time());
        if(WantedWeapon!=ShownWeapon)
        {
            SeniorWeaponPresentation::Destroy(HeldWeapon.Get()); HeldWeapon.Reset();
            ShownWeapon=WantedWeapon;
            if(!(Braxton.IsValid() && WantedWeapon==0))
            {
                auto* Body=Braxton.IsValid()?Braxton->GetBodyMesh():Mesh.Get();
                AActor* Owner=Braxton.IsValid()?Braxton->GetNativeCharacter():PreviewActor.Get();
                HeldWeapon=SeniorWeaponPresentation::Build(Owner,Body,WantedCharacter,WantedWeapon);
                HeldWeapon->AttachToComponent(Body,FAttachmentTransformRules::KeepRelativeTransform,TEXT("hand_r"));
                const auto& Ref=Body->GetSkeletalMeshAsset()->GetRefSkeleton();
                const FQuat Hand=Braxton.IsValid()?
                    FSeniorDouliIdle::At(0).Rotation*FSeniorDouliIdle::PalmFrame()*FSeniorDouliIdle::HandFrame(Ref).Inverse():
                    Body->GetSocketTransform(TEXT("hand_r"),RTS_Component).GetRotation();
                const FQuat Grip=Hand.Inverse()*FRotator(-65,Braxton.IsValid()?90:0,0).Quaternion();
                const FVector PalmOffset=Braxton.IsValid()?FVector(0,6,0):FVector(4,0,0);
                const FVector SocketScale=Body->GetSocketTransform(TEXT("hand_r"),RTS_Component).GetScale3D();
                const FVector GripOffset=(Hand.Inverse().RotateVector(PalmOffset)+Grip.RotateVector(FVector(0,0,4)))/SocketScale;
                HeldWeapon->SetRelativeTransform(FTransform(Grip,GripOffset));
                // Legacy Blender rigs carry import-unit scale on the hand bone.
                // Cosmetic props are authored in centimetres, never bone units.
                HeldWeapon->SetAbsolute(false,false,true);
                HeldWeapon->SetWorldScale3D(FVector::OneVector);
#if WITH_EDITOR
                UE_LOG(LogTemp,Display,TEXT("ARMORY_HELD: character=%d weapon=%d bone=%s prop=%s"),WantedCharacter,WantedWeapon,
                    *Body->GetSocketTransform(TEXT("hand_r"),RTS_Component).ToHumanReadableString(),*HeldWeapon->GetComponentTransform().ToHumanReadableString());
#endif
                Capture->ShowOnlyActorComponents(Owner);
            }
        }
        if (Braxton.IsValid())
        {
            if(WantedWeapon==1 && !Cast<USeniorDouliAnim>(Braxton->GetBodyMesh()->GetAnimInstance()))
                Braxton->GetBodyMesh()->SetAnimInstanceClass(USeniorDouliAnim::StaticClass());
            Braxton->SetDouliEquipped(WeaponIndex.Get(0)==0);
            if(auto* Anim=Cast<USeniorDouliAnim>(Braxton->GetBodyMesh()->GetAnimInstance()))
            {
                Anim->bSelectionRoom=bInteractive;
                Anim->SelectionRoomTime=FSeniorSelectionRoom::Time();
                if(WantedWeapon==1)
                {
                    // Relaxed low carry for the prototype, never the hat-toss cycle.
                    Anim->bEquipped=true; Anim->bLobbyIdle=true; Anim->LobbyTime=0;
                }
            }
            Capture->ShowOnlyActorComponents(Braxton->GetNativeCharacter());
        }
        if (Braxton.IsValid()) Braxton->KeepPreviewActive();
        // Scene captures are isolated far from the play camera, so their texture demand is not
        // represented by the normal world streaming view. Keep a short lease only while painted.
        // It expires automatically on hide, character change, travel, or widget destruction.
        if (Now - LastPrestreamTime >= 1.0)
        {
            Mesh->PrestreamTextures(3.0f, false);
            if (Braxton.IsValid()) Braxton->PrestreamPreviewTextures();
            LastPrestreamTime = Now;
        }
        const double FrameInterval = bInteractive ? 1.0 / 30.0 : 1.0 / 15.0;
        if (!bNeedsCapture && Now - LastCaptureTime < FrameInterval) return;

        USkeletalMeshComponent* Body = Mesh.Get();
#if WITH_EDITOR
        if (bInteractive && FParse::Param(FCommandLine::Get(), TEXT("LobbyPreviewTurnDemo")))
            RotationYaw = -8.f + 75.f * FMath::Sin(float(Now - TurnDemoStarted) * 1.7f);
        if (bInteractive && FParse::Param(FCommandLine::Get(), TEXT("LobbyPreviewShakeDemo")))
            RotationYaw = 90.f * FMath::Sin(float(Now - TurnDemoStarted) * 12.f);
#endif
        if (Braxton.IsValid())
        {
            const float CurrentYaw=Braxton->GetRootComponent()->GetRelativeRotation().Yaw;
            const float TurnStep=FMath::Clamp(FMath::FindDeltaAngleDegrees(CurrentYaw,RotationYaw),-360.f*DeltaTime,360.f*DeltaTime);
            Braxton->SetActorRelativeRotation(FRotator(0,CurrentYaw+TurnStep,0));
            if(bRoom)
            {
                // Enlarge only the selection-room presentation, keeping feet on the slab.
                // The room camera, main-lobby lineup and gameplay character are unchanged.
                Braxton->SetActorRelativeLocation(FVector(40,0,0));
                Braxton->SetActorRelativeScale3D(FVector(1.18f));
            }
        }
        else
        {
            Body->SetForcedLOD(bInteractive && Zoom < .75f ? 1 : 0);
            if (bHasAnimation)
            {
                // Component ticking is disabled. Hidden previews do not keep animating in the world.
                const float AnimationStep = float(FMath::Clamp(Now - LastCaptureTime, 0.0, 0.1));
                Body->TickAnimation(AnimationStep, false);
            }
            ClothMotion.Update(Body, float(Now - LastCaptureTime), RotationYaw);
            Body->RefreshBoneTransforms();
            Body->SetRelativeRotation(FRotator(0, RotationYaw, 0));
        }
        const float FaceBlend = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp((1.0f - Zoom) / (1.0f - FaceZoom), 0.0f, 1.0f));
        const bool bHatShowcase=bInteractive && Braxton.IsValid();
        // Leave space on the prop side for the lobby toss without camera bobbing.
        // Manual face zoom retains the original close-up framing.
        Capture->OrthoWidth = FullBodyOrthoWidth * Zoom * (bHatShowcase?1.f+.26f*(1.f-FaceBlend):1.f);
        const float CameraDistance = Braxton.IsValid() ? 10.f + Capture->OrthoWidth / (2.f * FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle*.5f))) : 400.f;
        const float ShowcaseSide=0;
        Capture->SetRelativeLocation(FVector(CameraDistance, ShowcaseSide, FMath::Clamp(FMath::Lerp(FullBodyAimZ, FaceAimZ, FaceBlend) + PanOffset, 15.f, 190.f)));
#if WITH_EDITOR
        if (bInteractive && Braxton.IsValid() && FParse::Param(FCommandLine::Get(),TEXT("LobbyPreviewGrip")))
        {
            const FName GripBone=FParse::Param(FCommandLine::Get(),TEXT("LobbyPreviewGripLeft"))?FName(TEXT("hand_l")):FName(TEXT("hand_r"));
            const FVector Grip=PreviewActor->GetActorTransform().InverseTransformPosition(Braxton->GetBodyMesh()->GetSocketLocation(GripBone));
            Capture->SetRelativeLocation(Grip+FVector(CameraDistance,0,0));
        }
#endif
        if(bInteractive && !bRoom)
        {
            TInlineComponentArray<UPointLightComponent*> Lights(PreviewActor.Get());
            for(UPointLightComponent* Light:Lights)
                if(Light->GetFName()==TEXT("PreviewKey"))
                    Light->SetIntensity(48.f*FSeniorSelectionRoom::Light(FSeniorSelectionRoom::Time()));
        }
        if(bRoom)
        {
            Capture->ProjectionType=ECameraProjectionMode::Perspective;
            Capture->FOVAngle=55;
            Capture->SetRelativeLocation(FVector(660*Zoom,0,180+PanOffset));
            Capture->SetRelativeRotation(FRotator(-5,180,0));
#if WITH_EDITOR
            float ReviewAngle=0;
            if(FParse::Value(FCommandLine::Get(),TEXT("RoomPreviewAngle="),ReviewAngle))
            {
                Capture->SetRelativeLocation(FRotator(0,ReviewAngle,0).RotateVector(FVector(660*Zoom,0,0))+FVector(0,0,180+PanOffset));
                Capture->SetRelativeRotation(FRotator(-5,180+ReviewAngle,0));
            }
            if(!bRoomValidated && Room.Validate())
            {
                bRoomValidated=true;
                UE_LOG(LogTemp,Display,TEXT("CHARACTER_ROOM_VERIFIED: world-space writing, real shelf props grounded, shelf/sign ray targets valid"));
            }
#endif
            Capture->ShowFlags.SetDynamicShadows(true);
            Capture->ShowOnlyActorComponents(PreviewActor.Get());
            TInlineComponentArray<UPointLightComponent*> Lights(PreviewActor.Get());
            for(auto* Light:Lights)if(Light->GetName().StartsWith(TEXT("Preview")))Light->SetIntensity(0);
        }
        Capture->CaptureScene();
#if WITH_EDITOR
        if(bRoom && FParse::Param(FCommandLine::Get(),TEXT("RoomSelectionReview")))
        {
            // Exercise the same screen-to-world selection path as a real click,
            // across separate rendered frames, without saving a party loadout.
            auto ClickShelf=[&](int32 Index)
            {
                const FVector Point=PreviewActor->GetActorLocation()+FVector(9,-215,Index==0?180:106);
                const FVector View=Capture->GetComponentTransform().InverseTransformPosition(Point);
                const float Half=View.X*FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle*.5f));
                const FVector2D Size=Geometry.GetLocalSize();
                const FVector2D Pixel(Size.X*(.5+View.Y/(2*Half)),Size.Y*.5-View.Z*Size.X/(2*Half));
                return SelectRoomObject(Geometry,Geometry.LocalToAbsolute(Pixel));
            };
            const double Elapsed=Now-TurnDemoStarted;
            if(RoomReviewStage==0 && Elapsed>12)
            {
                RoomReviewOriginal=WantedWeapon;
                ensureAlwaysMsgf(ClickShelf(1),TEXT("Shelf screen-space hit failed"));RoomReviewStage=1;
            }
            else if(RoomReviewStage==1 && Elapsed>16)
            {
                ensureAlwaysMsgf(WeaponIndex.Get()==1 && HeldWeapon.IsValid(),TEXT("Second shelf weapon did not equip"));
                ensureAlwaysMsgf(ClickShelf(0),TEXT("Top shelf screen-space hit failed"));RoomReviewStage=2;
            }
            else if(RoomReviewStage==2 && Elapsed>20)
            {
                if(WeaponIndex.Get()==0 && !HeldWeapon.IsValid())
                { UE_LOG(LogTemp,Display,TEXT("CHARACTER_ROOM_SELECTION_PASSED: projected shelf clicks equipped both weapons")); }
                else { UE_LOG(LogTemp,Error,TEXT("CHARACTER_ROOM_SELECTION_FAILED")); }
                if(OnWeapon)OnWeapon(RoomReviewOriginal);RoomReviewStage=3;
            }
        }
#endif
        LastCaptureTime = Now;
        bNeedsCapture = false;
    }

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
        FSlateWindowElementList& DrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
    {
        LastPaintTime = FPlatformTime::Seconds();
        if(!bInteractive && Braxton.IsValid() && Capture.IsValid())
        {
            // Contact shadows use the same camera projection as the feet, so
            // they remain beneath the sandals throughout the idle animation.
            static const FSlateRoundedBoxBrush ContactBrush(FLinearColor::White,100.f);
            const FVector2f Size(Geometry.GetLocalSize());
            const auto* NativeBody=Braxton->GetBodyMesh();
            if(NativeBody)
            {
                FVector2f Feet[2];
                int32 FootIndex=0;
                for(const TCHAR* Bone:{TEXT("ball_l"),TEXT("ball_r")})
                {
                    FVector P=NativeBody->GetSocketLocation(Bone); P.Z-=2.f;
                    const FVector View=Capture->GetComponentTransform().InverseTransformPosition(P);
                    const float HalfWidth=FMath::Max(1.f,float(View.X)*FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle*.5f)));
                    const FVector2f Centre(Size.X*(.5f+float(View.Y)/(2*HalfWidth)),Size.Y*.5f-float(View.Z)*Size.X/(2*HalfWidth));
                    Feet[FootIndex++]=Centre;
                    for(int32 Ring=0;Ring<7;++Ring)
                    {
                        const float Spread=1.f-float(Ring)*.105f;
                        const FVector2f Extent(Size.X*.112f*Spread,Size.Y*.033f*Spread);
                        FSlateDrawElement::MakeBox(DrawElements,LayerId,
                            Geometry.ToPaintGeometry(Extent,FSlateLayoutTransform(Centre-Extent*.5f)),
                            &ContactBrush,ESlateDrawEffect::None,FLinearColor(.015f,.012f,.009f,.14f));
                    }
                }
                const FVector2f Centre=(Feet[0]+Feet[1])*.5f;
                for(int32 Ring=0;Ring<5;++Ring)
                {
                    const float Spread=1.f-float(Ring)*.1f;
                    const FVector2f Extent(Size.X*.32f*Spread,Size.Y*.05f*Spread);
                    FSlateDrawElement::MakeBox(DrawElements,LayerId,
                        Geometry.ToPaintGeometry(Extent,FSlateLayoutTransform(Centre-Extent*.5f)),
                        &ContactBrush,ESlateDrawEffect::None,FLinearColor(.015f,.012f,.009f,.055f));
                }
            }
        }
        if(bInteractive && !bRoom && ShadowMaterial.IsValid() && Zoom>.85f)
        {
            const FVector2f Size(Geometry.GetLocalSize());
            // Project the live alpha silhouette, not a baked character image.
            // Several low-opacity offset taps soften the wall shadow's edge.
            for(int32 I=0;I<5;++I)
                FSlateDrawElement::MakeBox(DrawElements,LayerId,
                    Geometry.ToPaintGeometry(Size,FSlateLayoutTransform(FVector2f(10+I*1.8f,4+I))),
                    &ShadowBrush,ESlateDrawEffect::None,FLinearColor::White);
            FSlateDrawElement::MakeBox(DrawElements,LayerId,
                Geometry.ToPaintGeometry(FVector2f(Size.X,Size.Y*.065f),FSlateLayoutTransform(FVector2f(0,Size.Y*.873f))),
                &ShadowBrush,ESlateDrawEffect::None,FLinearColor::White);
        }
        return SCompoundWidget::OnPaint(Args, Geometry, CullingRect, DrawElements, LayerId+1, Style, bParentEnabled);
    }

    virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        if(bRoom && Event.GetEffectingButton()==EKeys::LeftMouseButton && SelectRoomObject(Geometry,Event.GetScreenSpacePosition()))return FReply::Handled();
        if (!bInteractive || (Event.GetEffectingButton() != EKeys::LeftMouseButton && Event.GetEffectingButton() != EKeys::RightMouseButton)) return FReply::Unhandled();
        bPanning = Event.GetEffectingButton() == EKeys::RightMouseButton;
        bDragging = true;
        LastDragPosition = Event.GetScreenSpacePosition();
        SetCursor(EMouseCursor::GrabHandClosed);
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }

    virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        if (!bDragging || !HasMouseCapture()) return FReply::Unhandled();
        if (bPanning)
        {
            PanOffset = FMath::Clamp(PanOffset + float(Event.GetScreenSpacePosition().Y - LastDragPosition.Y) * .20f * Zoom, -150.f, 80.f);
            LastDragPosition = Event.GetScreenSpacePosition(); bNeedsCapture = true;
        }
        else RotateTo(Event.GetScreenSpacePosition());
        return FReply::Handled();
    }

    virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        if (!bDragging || (Event.GetEffectingButton() != EKeys::LeftMouseButton && Event.GetEffectingButton() != EKeys::RightMouseButton)) return FReply::Unhandled();
        bDragging = false;
        bPanning = false;
        SetCursor(EMouseCursor::GrabHand);
        return FReply::Handled().ReleaseMouseCapture();
    }

    virtual void OnMouseCaptureLost(const FCaptureLostEvent& Event) override
    {
        bDragging = false;
        bPanning = false;
        if (bInteractive) SetCursor(EMouseCursor::GrabHand);
        SCompoundWidget::OnMouseCaptureLost(Event);
    }

    virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        if (!bInteractive) return FReply::Unhandled();
        Zoom = FMath::Clamp(Zoom * FMath::Pow(0.83f, Event.GetWheelDelta()), FaceZoom, 1.15f);
        bNeedsCapture = true;
        return FReply::Handled();
    }

    virtual FReply OnTouchGesture(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        if (!bInteractive || Event.GetGestureType() != EGestureEvent::Magnify) return FReply::Unhandled();
        Zoom = FMath::Clamp(Zoom * FMath::Exp(-Event.GetGestureDelta().X), FaceZoom, 1.15f);
        bNeedsCapture = true;
        return FReply::Handled();
    }

    virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        if (!bInteractive || Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
        RotationYaw = -8.0f;
        TurnDemoStarted = FPlatformTime::Seconds();
        Zoom = 1.0f;
        PanOffset = 0;
        ClothMotion.Bind(Mesh.Get(), RotationYaw);
        bNeedsCapture = true;
        return FReply::Handled();
    }

    virtual FReply OnTouchStarted(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        if(bRoom && SelectRoomObject(Geometry,Event.GetScreenSpacePosition()))return FReply::Handled();
        if (!bInteractive) return FReply::Unhandled();
        bDragging = true;
        bPanning = false;
        LastDragPosition = Event.GetScreenSpacePosition();
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }

    virtual FReply OnTouchMoved(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        if (!bDragging) return FReply::Unhandled();
        RotateTo(Event.GetScreenSpacePosition());
        return FReply::Handled();
    }

    virtual FReply OnTouchEnded(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        if (!bDragging) return FReply::Unhandled();
        bDragging = false;
        return FReply::Handled().ReleaseMouseCapture();
    }

private:
    void RotateTo(const FVector2D& Position)
    {
        RotationYaw = FMath::UnwindDegrees(RotationYaw + (Position.X - LastDragPosition.X) * 0.45f);
        LastDragPosition = Position;
        bNeedsCapture = true;
    }

    void CreateScene(UWorld* World)
    {
        UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,bRoom?
            TEXT("/Game/Lobby/CharacterRoom/M_RoomViewport.M_RoomViewport"):
            TEXT("/Game/Characters/UI/M_CharacterPreview.M_CharacterPreview"));
        if (!Material)
        {
            UE_LOG(LogTemp, Error, TEXT("Senior character preview: UI material M_CharacterPreview is missing."));
            bCreationFailed = true;
            return;
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.ObjectFlags = RF_Transient;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        static uint32 SceneSerial = 0;
        const FVector SceneOrigin(0, 0, -100000.0 - 2500.0 * (++SceneSerial % 1000));
        AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), SceneOrigin, FRotator::ZeroRotator, SpawnParameters);
        if (!Actor)
        {
            bCreationFailed = true;
            return;
        }
        PreviewActor = Actor;
        Actor->Tags.Add(TEXT("SeniorCharacterPreview"));
        Actor->SetReplicates(false);
        Actor->SetActorEnableCollision(false);
        Actor->SetActorTickEnabled(false);

        USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("PreviewRoot"));
        Actor->SetRootComponent(Root);
        Actor->AddInstanceComponent(Root);
        Root->RegisterComponent();
        Actor->SetActorLocation(SceneOrigin);
        if(bRoom)Room.Build(Actor);

        USkeletalMeshComponent* Body = NewObject<USkeletalMeshComponent>(Actor, TEXT("PreviewBody"));
        Actor->AddInstanceComponent(Body);
        Body->SetupAttachment(Root);
        Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Body->SetCastShadow(false);
        Body->SetVisibleInSceneCaptureOnly(true);
        Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        Body->RegisterComponent();
        Body->SetComponentTickEnabled(false);
        Mesh = Body;

        AddLight(Actor, Root, TEXT("PreviewKey"), FVector(200, -130, 220), FLinearColor(1.0f, 0.91f, 0.79f), 48.0f);
        AddLight(Actor, Root, TEXT("PreviewFill"), FVector(170, 180, 125), FLinearColor(0.75f, 0.84f, 1.0f), 18.0f);
        AddLight(Actor, Root, TEXT("PreviewRim"), FVector(-130, 40, 190), FLinearColor(0.90f, 0.95f, 1.0f), 30.0f);

        RenderTarget.Reset(NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient));
        RenderTarget->ClearColor = FLinearColor(0, 0, 0, 1);
        RenderTarget->RenderTargetFormat = RTF_RGBA16f;
        RenderTarget->bForceLinearGamma = true;
        // SceneColor+alpha captures bypass post-process AA. Filter their high-
        // resolution image down to the widget instead of point-sampling fine hairs.
        RenderTarget->bAutoGenerateMips = bInteractive;
        RenderTarget->Filter = TF_Trilinear;
        RenderTarget->MipsSamplerFilter = TF_Bilinear;
        RenderTarget->InitAutoFormat(bInteractive ? 1024 : 768, bInteractive ? 1024 : 768);
        RenderTarget->UpdateResourceImmediate(true);

        USceneCaptureComponent2D* Camera = NewObject<USceneCaptureComponent2D>(Actor, TEXT("PreviewCamera"));
        Actor->AddInstanceComponent(Camera);
        Camera->SetupAttachment(Root);
        Camera->ProjectionType = ECameraProjectionMode::Orthographic;
        Camera->OrthoWidth = FullBodyOrthoWidth;
        Camera->bAutoCalculateOrthoPlanes = false;
        Camera->SetRelativeLocation(FVector(400, 0, FullBodyAimZ));
        Camera->SetRelativeRotation(FRotator(0, 180, 0));
        Camera->TextureTarget = RenderTarget.Get();
        Camera->CaptureSource = bRoom?SCS_FinalToneCurveHDR:SCS_SceneColorHDR;
        Camera->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
        Camera->bCaptureEveryFrame = false;
        Camera->bCaptureOnMovement = false;
        Camera->bAlwaysPersistRenderingState = bRoom;
        Camera->bUseRayTracingIfEnabled = false;
        Camera->MaxViewDistanceOverride = 1000;
        Camera->ShowFlags.SetAtmosphere(false);
        Camera->ShowFlags.SetFog(false);
        Camera->ShowFlags.SetVolumetricFog(false);
        Camera->ShowFlags.SetSkyLighting(false);
        Camera->ShowFlags.SetBloom(bRoom);
        Camera->ShowFlags.SetEyeAdaptation(false);
        Camera->ShowFlags.SetMotionBlur(false);
        Camera->ShowFlags.SetTemporalAA(bRoom);
        if(bRoom)Camera->ShowFlags.SetAntiAliasing(true);
        Camera->ShowFlags.SetDynamicShadows(false);
        Camera->ShowFlags.SetAmbientOcclusion(false);
        Camera->ShowFlags.SetScreenSpaceReflections(false);
        Camera->ShowFlags.SetLumenGlobalIllumination(false);
        Camera->ShowFlags.SetLumenReflections(false);
        Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
        Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
        Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
        Camera->PostProcessSettings.AutoExposureBias = 0;
        Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
        Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
        Camera->ShowOnlyComponent(Body);
        Camera->RegisterComponent();
        Camera->SetComponentTickEnabled(false);
        Capture = Camera;

        PreviewMaterial.Reset(UMaterialInstanceDynamic::Create(Material, GetTransientPackage()));
        PreviewMaterial->SetTextureParameterValue(TEXT("PreviewTexture"), RenderTarget.Get());
        Brush.SetResourceObject(PreviewMaterial.Get());
        if(bInteractive)
            if(auto* Shadow=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Characters/UI/M_SelectionShadow.M_SelectionShadow")))
            {
                ShadowMaterial.Reset(UMaterialInstanceDynamic::Create(Shadow,GetTransientPackage()));
                ShadowMaterial->SetTextureParameterValue(TEXT("PreviewTexture"),RenderTarget.Get());
                ShadowBrush.DrawAs=ESlateBrushDrawType::Image;
                ShadowBrush.SetResourceObject(ShadowMaterial.Get());
            }
        bNeedsCapture = true;
    }

    static void AddLight(AActor* Actor, USceneComponent* Root, FName Name, const FVector& Location,
        const FLinearColor& Color, float Intensity)
    {
        UPointLightComponent* Light = NewObject<UPointLightComponent>(Actor, Name);
        Actor->AddInstanceComponent(Light);
        Light->SetupAttachment(Root);
        Light->SetRelativeLocation(Location);
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetUseInverseSquaredFalloff(false);
        Light->SetIntensity(Intensity);
        Light->SetLightColor(Color, false);
        Light->SetAttenuationRadius(650);
        Light->SetCastShadows(Name == TEXT("PreviewKey"));
        Light->RegisterComponent();
        Light->SetComponentTickEnabled(false);
    }

    void SetCharacter(int32 Index)
    {
        SeniorWeaponPresentation::Destroy(HeldWeapon.Get()); HeldWeapon.Reset(); ShownWeapon=-1;
        if (Braxton.IsValid()) { Braxton->Destroy(); Braxton.Reset(); }
        Capture->ClearShowOnlyComponents();
        Capture->ShowOnlyComponent(Mesh.Get());
        Capture->ProjectionType = ECameraProjectionMode::Orthographic;
        TInlineComponentArray<UPointLightComponent*> PreviewLights(PreviewActor.Get());
        for (UPointLightComponent* Light : PreviewLights)
        {
            Light->SetIntensity(Light->GetFName() == TEXT("PreviewKey") ? 48.f : Light->GetFName() == TEXT("PreviewFill") ? 18.f : 30.f);
            Light->SetSourceRadius(0.f);
            Light->SetSoftSourceRadius(0.f);
        }
        ShownCharacter = Index;
        USkeletalMeshComponent* Body = Mesh.Get();
        USkeletalMesh* Asset = SeniorRoster::Body(Index);
        if (!Body || !Asset)
        {
            if (Body) Body->SetVisibility(false);
            UE_LOG(LogTemp, Error, TEXT("Senior character preview: body %d is missing."), Index + 1);
            bNeedsCapture = true;
            return;
        }
        Body->SetVisibility(true);
        Body->SetSkeletalMesh(Asset);
        Body->SetCastShadow(Index == 0);
        Capture->ShowFlags.SetDynamicShadows(Index == 0);
        // Higher preview resolution reveals the pores/fabric while zooming.
        RenderTarget->ResizeTarget(bInteractive && Index == 0 ? 1536 : bInteractive ? 1024 : 768,
            bInteractive && Index == 0 ? 1536 : bInteractive ? 1024 : 768);
        if(bRoom)RenderTarget->ResizeTarget(1600,900);
        UAnimSequence* Idle = SeniorRoster::Idle(Index);
        bHasAnimation = Idle != nullptr;
        if (Idle) Body->PlayAnimation(Idle, true);
        Body->SetComponentTickEnabled(false);
        Body->TickAnimation(0, false);
        Body->RefreshBoneTransforms();
        const FBoxSphereBounds Bounds = Asset->GetBounds();
        const float Height = FMath::Max(float(Bounds.BoxExtent.Z * 2.0), 120.0f);
        const float LowestPoint = float(Bounds.Origin.Z - Bounds.BoxExtent.Z);
        Body->SetRelativeLocation(FVector(bRoom?40:0, 0, -LowestPoint));
        FaceAimZ = Height - FMath::Clamp(Height * 0.075f, 12.0f, 15.0f);
        RotationYaw = -8.0f;
        TurnDemoStarted = FPlatformTime::Seconds();
        Zoom = 1.0f;
        PanOffset = 0;
        ClothMotion.Bind(Body, RotationYaw);
        LastPrestreamTime = 0;
        // The original visual remains available as a recovery/debug option.
        if (Index == 0 && !FParse::Param(FCommandLine::Get(), TEXT("BraxtonLegacyVisual")))
        {
            FActorSpawnParameters Params;
            Params.ObjectFlags = RF_Transient;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ASeniorBraxtonVisual* NewVisual = PreviewWorld->SpawnActor<ASeniorBraxtonVisual>(
                PreviewActor->GetActorLocation(), FRotator::ZeroRotator, Params);
            if (NewVisual && NewVisual->InitializeVisual(true, false))
            {
                Braxton = NewVisual;
                NewVisual->AttachToActor(PreviewActor.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                Body->SetVisibility(false);
                Capture->ShowOnlyActorComponents(NewVisual->GetNativeCharacter());
                Capture->ProjectionType = ECameraProjectionMode::Perspective;
                Capture->FOVAngle = FMath::RadiansToDegrees(2.f*FMath::Atan(2.f*FMath::Tan(FMath::DegreesToRadians(9.f))));
                TInlineComponentArray<UPointLightComponent*> Lights(PreviewActor.Get());
                for (UPointLightComponent* Light : Lights)
                {
                    Light->SetIntensity(Light->Intensity * .28f);
                    Light->SetSourceRadius(12.f);
                    Light->SetSoftSourceRadius(22.f);
                }
                FaceAimZ = 163.f;
                bHasAnimation = false;
                NewVisual->KeepPreviewActive();
            }
            else if (NewVisual) NewVisual->Destroy();
        }
#if WITH_EDITOR
        if (bInteractive && FParse::Param(FCommandLine::Get(), TEXT("LobbyPreviewFace"))) Zoom = FaceZoom;
        FParse::Value(FCommandLine::Get(), TEXT("LobbyPreviewYaw="), RotationYaw);
        FParse::Value(FCommandLine::Get(), TEXT("LobbyPreviewZoom="), Zoom);
        FParse::Value(FCommandLine::Get(), TEXT("LobbyPreviewPan="), PanOffset);
#endif
        bNeedsCapture = true;
    }

    bool SelectRoomObject(const FGeometry& Geometry,FVector2D Screen)
    {
        if(!Capture.IsValid())return false;
        const FVector2D Local=Geometry.AbsoluteToLocal(Screen),Size=Geometry.GetLocalSize();
        const float Tan=FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle*.5f));
        const FVector Ray=Capture->GetComponentTransform().TransformVectorNoScale(FVector(1,(Local.X/Size.X*2-1)*Tan,(1-Local.Y/Size.Y*2)*Tan*Size.Y/Size.X)).GetSafeNormal();
        const int32 Hit=Room.Pick(Capture->GetComponentLocation(),Ray);
        if(Hit==2){OnBack.ExecuteIfBound();return true;}
        if(Hit>=0 && Hit<2){if(OnWeapon)OnWeapon(Hit);return true;}
        return false;
    }
    FSeniorCharacterRoom Room;
    bool bRoom=false;
    bool bRoomValidated=false;
    int32 RoomReviewStage=0,RoomReviewOriginal=0;
    TFunction<void(int32)> OnWeapon;
    FSimpleDelegate OnBack;
    TWeakObjectPtr<UWorld> PreviewWorld;
    double TurnDemoStarted = 0;
    TWeakObjectPtr<AActor> PreviewActor;
    TWeakObjectPtr<ASeniorBraxtonVisual> Braxton;
    TWeakObjectPtr<USceneComponent> HeldWeapon;
    int32 ShownWeapon=-1;
    TWeakObjectPtr<USkeletalMeshComponent> Mesh;
    TWeakObjectPtr<USceneCaptureComponent2D> Capture;
    TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget;
    TStrongObjectPtr<UMaterialInstanceDynamic> PreviewMaterial;
    TStrongObjectPtr<UMaterialInstanceDynamic> ShadowMaterial;
    FSlateBrush ShadowBrush;
    TAttribute<int32> CharacterIndex;
    TAttribute<int32> WeaponIndex;
    FSlateBrush Brush;
    FVector2D LastDragPosition = FVector2D::ZeroVector;
    mutable double LastPaintTime = 0;
    double LastCaptureTime = 0;
    double LastPrestreamTime = 0;
    int32 ShownCharacter = INDEX_NONE;
    float FaceAimZ = 170;
    float RotationYaw = -8;
    float Zoom = 1;
    float PanOffset = 0;
    FSeniorClothMotion ClothMotion;
    bool bInteractive = true;
    bool bPanning = false;
    bool bDragging = false;
    bool bHasAnimation = false;
    bool bNeedsCapture = true;
    bool bCreationFailed = false;
};
}

TSharedRef<SWidget> MakeSeniorCharacterPreview(UWorld* World, TAttribute<int32> CharacterIndex, bool bInteractive, TAttribute<int32> WeaponIndex)
{
    return SNew(SSeniorCharacterPreview).World(World).CharacterIndex(CharacterIndex).Interactive(bInteractive).WeaponIndex(WeaponIndex);
}
TSharedRef<SWidget> MakeSeniorCharacterRoomPreview(UWorld* World,TAttribute<int32> CharacterIndex,TAttribute<int32> WeaponIndex,TFunction<void(int32)> OnWeapon,FSimpleDelegate OnBack)
{
    return SNew(SSeniorCharacterPreview).World(World).CharacterIndex(CharacterIndex).Interactive(true).WeaponIndex(WeaponIndex).Room(true).OnWeapon(OnWeapon).OnBack(OnBack);
}
