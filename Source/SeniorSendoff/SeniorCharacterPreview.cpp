#include "SeniorCharacterPreview.h"
#include "SeniorCharacterRoster.h"
#include "SeniorClothMotion.h"
#include "SeniorBraxtonVisual.h"
#include "SeniorRunnerVisual.h"
#include "SeniorFixerVisual.h"
#include "SeniorDouliAnim.h"
#include "SeniorDouliIdle.h"
#include "SeniorSelectionRoom.h"
#include "SeniorWeaponPresentation.h"
#include "SeniorCharacterRoom.h"
#include "SeniorLobbyAtmosphere.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraTypes.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameUserSettings.h"
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
constexpr double RoomCharacterFadeSeconds = 0.18;

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
        SLATE_ARGUMENT(TFunction<void(int32)>, OnBrowse)
        SLATE_EVENT(FSimpleDelegate, OnBack)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        PreviewWorld = Args._World;
        CharacterIndex = Args._CharacterIndex;
        WeaponIndex = Args._WeaponIndex;
        bInteractive = Args._Interactive;
        bRoom=Args._Room; OnWeapon=Args._OnWeapon; OnBrowse=Args._OnBrowse; OnBack=Args._OnBack;
        // Slate draws an unbound image brush as a white rectangle. Keep the
        // lineup transparent until its first scene capture has been submitted.
        Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
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
        if (CachedBraxton.IsValid()) CachedBraxton->Destroy();
        if (Runner.IsValid()) Runner->Destroy();
        if (CachedRunner.IsValid()) CachedRunner->Destroy();
        if (Fixer.IsValid()) Fixer->Destroy();
        if (CachedFixer.IsValid()) CachedFixer->Destroy();
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
            if (Runner.IsValid()) Runner->SetVisualActive(false);
            if (Fixer.IsValid()) Fixer->SetVisualActive(false);
            return;
        }
        UWorld* World = PreviewWorld.Get();
        if (!World || World->bIsTearingDown || World->GetNetMode() == NM_DedicatedServer) return;
        if (!PreviewActor.IsValid() && !bCreationFailed) CreateScene(World);
        if (!Capture.IsValid() || !Mesh.IsValid()) return;

        // Browsing takes effect on the next tick, even while a blend is active.
        if (WantedCharacter != ShownCharacter)
        {
            // Keep the last complete room frame while the next character is
            // assembled and captured. The menu can then crossfade without a
            // black frame or an abrupt change to the figure and shelf text.
            if(bRoom && ShownCharacter!=INDEX_NONE && AlternateRenderTarget.IsValid() && IsSeniorLobbyMotionEnabled())
            {
                UTextureRenderTarget2D* OldTarget=ActiveRenderTarget;
                ActiveRenderTarget=ActiveRenderTarget==RenderTarget.Get()?AlternateRenderTarget.Get():RenderTarget.Get();
                Capture->TextureTarget=ActiveRenderTarget;
                TransitionMaterial->SetTextureParameterValue(TEXT("PreviewTexture"),OldTarget);
                bStartTransitionAfterCapture=true;
            }
            else { bStartTransitionAfterCapture=false; TransitionStarted=0; }
            SetCharacter(WantedCharacter);
        }
        const FIntPoint DesiredSize = GetPreviewSize(WantedCharacter);
        if (ActiveRenderTarget && (ActiveRenderTarget->SizeX != DesiredSize.X
            || ActiveRenderTarget->SizeY != DesiredSize.Y))
        {
            ActiveRenderTarget->ResizeTarget(DesiredSize.X, DesiredSize.Y);
            bNeedsCapture = true;
        }
        const int32 WantedWeapon=FMath::Clamp(WeaponIndex.Get(0),0,1);
        if(bRoom)Room.Update(ShownCharacter,WantedWeapon,FSeniorSelectionRoom::Time());
        if(WantedWeapon!=ShownWeapon)
        {
            SeniorWeaponPresentation::Destroy(HeldWeapon.Get()); HeldWeapon.Reset();
            ShownWeapon=WantedWeapon;
            if(!(Braxton.IsValid() && WantedWeapon==0))
            {
                auto* Body=Braxton.IsValid()?Braxton->GetBodyMesh():Runner.IsValid()?Runner->GetBodyMesh():Fixer.IsValid()?Fixer->GetBodyMesh():Mesh.Get();
                AActor* Owner=Braxton.IsValid()?Braxton->GetNativeCharacter():Runner.IsValid()?Runner->GetNativeCharacter():Fixer.IsValid()?Fixer->GetNativeCharacter():PreviewActor.Get();
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
                if(Braxton.IsValid() && WantedWeapon==1)
                {
                    // The sauce cup is held upright in his palm.  The general
                    // two-handed weapon grip puts its foil lid across his wrist.
                    const FTransform Palm=Body->GetSocketTransform(TEXT("hand_r"),RTS_World);
                    HeldWeapon->SetWorldTransform(FTransform(Owner->GetActorQuat(),
                        Palm.GetLocation()+Owner->GetActorQuat().RotateVector(FVector(3.f,-2.f,-1.f)),
                        FVector(1.35f)));
                }
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
            Braxton->SetDouliEquipped(WantedWeapon==0);
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
        if (Runner.IsValid())
        {
            Runner->KeepPreviewActive();
            Capture->ShowOnlyActorComponents(Runner->GetNativeCharacter());
        }
        if (Fixer.IsValid())
        {
            Fixer->KeepPreviewActive();
            if (HeldWeapon.IsValid() && Fixer->GetBodyMesh())
            {
                USkeletalMeshComponent* FixerBody=Fixer->GetBodyMesh();
                const FVector Wrist=FixerBody->GetSocketLocation(TEXT("hand_r"));
                const FVector Palm=FixerBody->DoesSocketExist(TEXT("middle_01_r"))?
                    FMath::Lerp(Wrist,FixerBody->GetSocketLocation(TEXT("middle_01_r")),.85f):Wrist-Fixer->GetActorUpVector()*6.f;
                const FVector Forward=Fixer->GetActorForwardVector();
                const FVector Right=Fixer->GetActorRightVector();
                const FVector Up=Fixer->GetActorUpVector();
                const float HandSide=FVector::DotProduct(Palm-Fixer->GetActorLocation(),Right)<0.f?-1.f:1.f;
                const FVector Outward=Right*HandSide;
                const FVector CarryDirection=(Forward*(WantedWeapon==0?.18f:.32f)+
                    Outward*(WantedWeapon==0?.24f:.26f)-Up*.95f).GetSafeNormal();
                const FQuat CarryRotation=FRotationMatrix::MakeFromXZ(CarryDirection,Forward).ToQuat();
                const FVector LocalGrip=WantedWeapon==0?FVector(4.5f,0,0):FVector(-2,0,-3);
                const FVector PalmOffset=Outward*.6f+Forward*.5f;
                // Read the animated hand every tick, after the native pose is
                // available. A bind-pose grip left the long pipe across his hips.
                // Use the wrapper's +X forward convention for a relaxed carry
                // beside the thigh, including while turning the character.
                HeldWeapon->SetWorldLocationAndRotation(Palm+PalmOffset-CarryRotation.RotateVector(LocalGrip),CarryRotation);
                HeldWeapon->SetWorldScale3D(FVector::OneVector);
            }
            Capture->ShowOnlyActorComponents(Fixer->GetNativeCharacter());
        }
        // Scene captures are isolated far from the play camera, so their texture demand is not
        // represented by the normal world streaming view. Keep a short lease only while painted.
        // It expires automatically on hide, character change, travel, or widget destruction.
        if (Now - LastPrestreamTime >= 1.0)
        {
            Mesh->PrestreamTextures(3.0f, false);
            if (Braxton.IsValid()) Braxton->PrestreamPreviewTextures();
            if (Runner.IsValid()) Runner->PrestreamPreviewTextures();
            if (Fixer.IsValid()) Fixer->PrestreamPreviewTextures();
            LastPrestreamTime = Now;
        }
        // Scene captures render a second view of an animated, groomed character.
        // Match their cadence to the player's quality preset so the lobby does
        // not consume the frame budget before the player enters the house.
        const int32 Quality = GetPreviewQuality();
        const double FrameInterval = bInteractive
            ? 1.0 / (Quality >= 3 ? 24.0 : Quality >= 2 ? 18.0 : 12.0)
            : 1.0 / (Quality >= 2 ? 12.0 : 8.0);
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
                Braxton->SetActorRelativeLocation(FVector(100,0,1.0f));
                Braxton->SetActorRelativeScale3D(FVector(1.23f));
            }
        }
        else if (Runner.IsValid())
        {
            const float CurrentYaw=Runner->GetRootComponent()->GetRelativeRotation().Yaw;
            const float TurnStep=FMath::Clamp(FMath::FindDeltaAngleDegrees(CurrentYaw,RotationYaw),-360.f*DeltaTime,360.f*DeltaTime);
            Runner->SetActorRelativeRotation(FRotator(0,CurrentYaw+TurnStep,0));
            if(bRoom)
            {
                Runner->SetActorRelativeLocation(FVector(100,0,1.0f));
                Runner->SetActorRelativeScale3D(FVector(1.23f));
            }
        }
        else if (Fixer.IsValid())
        {
            const float CurrentYaw=Fixer->GetRootComponent()->GetRelativeRotation().Yaw;
            const float TurnStep=FMath::Clamp(FMath::FindDeltaAngleDegrees(CurrentYaw,RotationYaw),-360.f*DeltaTime,360.f*DeltaTime);
            Fixer->SetActorRelativeRotation(FRotator(0,CurrentYaw+TurnStep,0));
            if(bRoom)
            {
                Fixer->SetActorRelativeLocation(FVector(100,0,1.0f));
                Fixer->SetActorRelativeScale3D(FVector(1.23f));
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
        const float CameraDistance = (Braxton.IsValid() || Runner.IsValid() || Fixer.IsValid()) ? 10.f + Capture->OrthoWidth / (2.f * FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle*.5f))) : 400.f;
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
            // The room turntable has a larger presentation-only character scale.
            // Move a close-up toward the head as well as toward the character;
            // simply shortening the camera distance framed the jersey instead.
            const float RoomFaceBlend = FMath::SmoothStep(0.f, 1.f,
                FMath::Clamp((1.f-Zoom)/(1.f-FaceZoom),0.f,1.f));
            const float RoomCameraX = FMath::Lerp(660.f*Zoom,100.f+350.f*Zoom,RoomFaceBlend)+
                (Fixer.IsValid()?10.f*RoomFaceBlend:0.f);
            // A face close-up should be near the eyes, not looking down from
            // above the forehead. Keep the full-body camera untouched.
            const float RoomEyeZ=Fixer.IsValid()?FaceAimZ*1.23f-1.f:245.f;
            Capture->SetRelativeLocation(FVector(RoomCameraX,0,FMath::Lerp(180.f,RoomEyeZ,RoomFaceBlend)+PanOffset));
            Capture->SetRelativeRotation(FRotator(FMath::Lerp(-5.f,-3.f,RoomFaceBlend),180,0));
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
#if WITH_EDITOR
            if(FParse::Param(FCommandLine::Get(),TEXT("RoomHoverReturnReview")))
            {
                const FVector Sign=PreviewActor->GetActorLocation()+FVector(-57,248,272);
                const FVector View=Capture->GetComponentTransform().InverseTransformPosition(Sign);
                const float Half=View.X*FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle*.5f));
                const FVector2D Size=Geometry.GetLocalSize();
                const FVector2D Pixel(Size.X*(.5f+View.Y/(2.f*Half)),Size.Y*.5f-View.Z*Size.X/(2.f*Half));
                const FVector2D Screen=Geometry.LocalToAbsolute(Pixel);
                ensureAlwaysMsgf(HitRoomObject(Geometry,Screen)==2,TEXT("Return sign hover target missed"));
                UpdateRoomHover(Geometry,Screen);
            }
            if(FParse::Param(FCommandLine::Get(),TEXT("RoomArrowReview")))
            {
                for(int32 Arrow=0;Arrow<2;++Arrow)
                {
                    const FVector Point=PreviewActor->GetActorLocation()+FSeniorCharacterRoom::ArrowPosition(Arrow);
                    const FVector View=Capture->GetComponentTransform().InverseTransformPosition(Point);
                    const float Half=View.X*FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle*.5f));
                    const FVector2D Size=Geometry.GetLocalSize();
                    const FVector2D Pixel(Size.X*(.5f+View.Y/(2.f*Half)),Size.Y*.5f-View.Z*Size.X/(2.f*Half));
                    const FVector2D Screen=Geometry.LocalToAbsolute(Pixel);
                    ensureAlwaysMsgf(HitRoomObject(Geometry,Screen)==Arrow+3,TEXT("Character arrow screen-space target missed"));
                    if(Arrow==0)UpdateRoomHover(Geometry,Screen);
                }
            }
#endif
            // A small, broad camera-side bounce can lift the under-eye shadows
            // for Runner's close-up without relighting the whiteboard or shelf.
            if(Runner.IsValid())
            {
                float RunnerFaceFill = 2.2f;
                FParse::Value(FCommandLine::Get(),TEXT("RunnerFaceFill="),RunnerFaceFill);
                for(auto* Light:Lights)if(Light->GetFName()==TEXT("PreviewFill"))
                {
                    Light->SetRelativeLocation(FVector(230,0,185));
                    Light->SetLightColor(FLinearColor(1.f,.94f,.87f),false);
                    Light->SetSourceRadius(80.f);
                    Light->SetSoftSourceRadius(100.f);
                    Light->SetIntensity(RunnerFaceFill);
                }
            }
        }
#if WITH_EDITOR
        const bool bMeasureCapture=bRoom && bStartTransitionAfterCapture && FParse::Param(FCommandLine::Get(),TEXT("RoomArrowCycleReview"));
        const double CaptureStarted=bMeasureCapture?FPlatformTime::Seconds():0;
#endif
        Capture->CaptureScene();
        Brush.DrawAs = ESlateBrushDrawType::Image;
#if WITH_EDITOR
        if(bMeasureCapture)
            UE_LOG(LogTemp,Display,TEXT("CHARACTER_ROOM_CAPTURE_MS: index=%d duration=%.1f"),ShownCharacter,(FPlatformTime::Seconds()-CaptureStarted)*1000.0);
#endif
        if(bStartTransitionAfterCapture)
        {
            // Start the fade after the expensive character setup and first
            // capture, so a cold asset load cannot consume the whole effect.
            PreviewMaterial->SetTextureParameterValue(TEXT("PreviewTexture"),ActiveRenderTarget);
            TransitionStarted=FPlatformTime::Seconds();
            bStartTransitionAfterCapture=false;
        }
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
        if(bRoom && FParse::Param(FCommandLine::Get(),TEXT("RoomArrowCycleReview")))
        {
            if(RoomArrowReviewStarted==0)RoomArrowReviewStarted=Now;
            auto ClickArrow=[&](int32 Arrow)
            {
                const FVector Point=PreviewActor->GetActorLocation()+FSeniorCharacterRoom::ArrowPosition(Arrow);
                const FVector View=Capture->GetComponentTransform().InverseTransformPosition(Point);
                const float Half=View.X*FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle*.5f));
                const FVector2D Size=Geometry.GetLocalSize();
                const FVector2D Pixel(Size.X*(.5f+View.Y/(2.f*Half)),Size.Y*.5f-View.Z*Size.X/(2.f*Half));
                return SelectRoomObject(Geometry,Geometry.LocalToAbsolute(Pixel));
            };
            const double Elapsed=Now-RoomArrowReviewStarted;
            if(RoomArrowReviewStage==0 && Elapsed>1 && ShownCharacter==0)
            {
                ensureAlwaysMsgf(ClickArrow(1) && CharacterIndex.Get()==1,TEXT("Right room arrow did not browse forward"));
                RoomArrowReviewStage=1;
            }
            else if(RoomArrowReviewStage==1 && ShownCharacter==1 && TransitionStarted>0)
            {
                ensureAlwaysMsgf(ClickArrow(1) && CharacterIndex.Get()==2,TEXT("Second right-arrow press during a fade was not accepted"));
                RoomArrowReviewStage=2;
            }
            else if(RoomArrowReviewStage==2 && ShownCharacter==2)
            {
                ensureAlwaysMsgf(ClickArrow(0) && CharacterIndex.Get()==1,TEXT("Left room arrow did not browse back"));
                RoomArrowReviewStage=3;
            }
            else if(RoomArrowReviewStage==3 && ShownCharacter==1)
            {
                ensureAlwaysMsgf(ClickArrow(0) && CharacterIndex.Get()==0,TEXT("Second left-arrow press during a fade was not accepted"));
                RoomArrowReviewStage=4;
            }
            else if(RoomArrowReviewStage==4 && ShownCharacter==0)
            {
                UE_LOG(LogTemp,Display,TEXT("CHARACTER_ROOM_ARROWS_PASSED: rapid wall-arrow presses browse without waiting for a fade"));
                RoomArrowReviewStage=5;
            }
        }
        if(bRoom && FParse::Param(FCommandLine::Get(),TEXT("RoomRosterReview")))
        {
            if(RoomRosterReviewStarted==0)RoomRosterReviewStarted=Now;
            auto ClickLamp=[&](int32 Index)
            {
                const FVector Point=PreviewActor->GetActorLocation()+FSeniorCharacterRoom::RosterLampPosition(Index);
                const FVector View=Capture->GetComponentTransform().InverseTransformPosition(Point);
                const float Half=View.X*FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle*.5f));
                const FVector2D Size=Geometry.GetLocalSize();
                const FVector2D Pixel(Size.X*(.5f+View.Y/(2.f*Half)),Size.Y*.5f-View.Z*Size.X/(2.f*Half));
                return SelectRoomObject(Geometry,Geometry.LocalToAbsolute(Pixel));
            };
            const double Elapsed=Now-RoomRosterReviewStarted;
            if(RoomRosterReviewStage==0 && Elapsed>4 && ShownCharacter==0)
            {
                ensureAlwaysMsgf(ClickLamp(1) && CharacterIndex.Get()==1,TEXT("Second roster lamp did not select Sam"));
                RoomRosterReviewStage=1;
            }
            else if(RoomRosterReviewStage==1 && Elapsed>8)
            {
                ensureAlwaysMsgf(ShownCharacter==1,TEXT("Roster lamp did not render Sam"));
                ensureAlwaysMsgf(ClickLamp(0) && CharacterIndex.Get()==0,TEXT("First roster lamp did not reselect Braxton"));
                RoomRosterReviewStage=2;
            }
            else if(RoomRosterReviewStage==2 && Elapsed>12)
            {
                ensureAlwaysMsgf(ShownCharacter==0,TEXT("Roster lamps did not return to Braxton"));
                UE_LOG(LogTemp,Display,TEXT("CHARACTER_ROOM_ROSTER_PASSED: inset lamps select both characters"));
                RoomRosterReviewStage=3;
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
        const int32 ContentLayer=SCompoundWidget::OnPaint(Args, Geometry, CullingRect, DrawElements, LayerId+1, Style, bParentEnabled);
        if(bRoom && TransitionStarted>0 && IsSeniorLobbyMotionEnabled() && TransitionMaterial.IsValid())
        {
            const float Progress=FMath::Clamp(float((FPlatformTime::Seconds()-TransitionStarted)/RoomCharacterFadeSeconds),0.f,1.f);
            if(Progress<1.f)
            {
                const float Opacity=1.f-FMath::SmoothStep(0.f,1.f,Progress);
                FSlateDrawElement::MakeBox(DrawElements,ContentLayer+1,
                    Geometry.ToPaintGeometry(FVector2f(Geometry.GetLocalSize()),FSlateLayoutTransform(FVector2f::ZeroVector)),
                    &TransitionBrush,ESlateDrawEffect::None,FLinearColor(1,1,1,Opacity));
                return ContentLayer+1;
            }
        }
        return ContentLayer;
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
        if(bRoom && !bDragging)UpdateRoomHover(Geometry,Event.GetScreenSpacePosition());
        if (!bDragging || !HasMouseCapture()) return FReply::Unhandled();
        if (bPanning)
        {
            PanOffset = FMath::Clamp(PanOffset + float(Event.GetScreenSpacePosition().Y - LastDragPosition.Y) * .20f * Zoom, -150.f, 80.f);
            LastDragPosition = Event.GetScreenSpacePosition(); bNeedsCapture = true;
        }
        else RotateTo(Event.GetScreenSpacePosition());
        return FReply::Handled();
    }

    virtual void OnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        SCompoundWidget::OnMouseEnter(Geometry,Event);
        if(bRoom && !bDragging)UpdateRoomHover(Geometry,Event.GetScreenSpacePosition());
    }

    virtual void OnMouseLeave(const FPointerEvent& Event) override
    {
        if(bRoom){Room.SetReturnHovered(false);Room.SetArrowHovered(-1);}
        if(bInteractive)SetCursor(EMouseCursor::GrabHand);
        SCompoundWidget::OnMouseLeave(Event);
    }

    virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        if (!bDragging || (Event.GetEffectingButton() != EKeys::LeftMouseButton && Event.GetEffectingButton() != EKeys::RightMouseButton)) return FReply::Unhandled();
        bDragging = false;
        bPanning = false;
        if(bRoom)UpdateRoomHover(Geometry,Event.GetScreenSpacePosition());
        else SetCursor(EMouseCursor::GrabHand);
        return FReply::Handled().ReleaseMouseCapture();
    }

    virtual void OnMouseCaptureLost(const FCaptureLostEvent& Event) override
    {
        bDragging = false;
        bPanning = false;
        if(bRoom){Room.SetReturnHovered(false);Room.SetArrowHovered(-1);}
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
        if(bRoom)
        {
            TArray<FSoftObjectPath> PreviewPaths;
            SeniorRoster::AppendSelectionPreviewPaths(PreviewPaths);
            RosterPreload=UAssetManager::GetStreamableManager().RequestAsyncLoad(MoveTemp(PreviewPaths));
        }

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

        auto CreateTarget=[this](TStrongObjectPtr<UTextureRenderTarget2D>& Target)
        {
            Target.Reset(NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient));
            Target->ClearColor = FLinearColor(0, 0, 0, 1);
            Target->RenderTargetFormat = RTF_RGBA16f;
            Target->bForceLinearGamma = true;
            // SceneColor+alpha captures bypass post-process AA; filter the
            // quality-scaled image down to the widget.
            Target->bAutoGenerateMips = bInteractive;
            Target->Filter = TF_Trilinear;
            Target->MipsSamplerFilter = TF_Bilinear;
            const FIntPoint Size = GetPreviewSize(CharacterIndex.Get(0));
            Target->InitAutoFormat(Size.X, Size.Y);
            Target->UpdateResourceImmediate(true);
        };
        CreateTarget(RenderTarget);
        if(bRoom)CreateTarget(AlternateRenderTarget);
        ActiveRenderTarget=RenderTarget.Get();

        USceneCaptureComponent2D* Camera = NewObject<USceneCaptureComponent2D>(Actor, TEXT("PreviewCamera"));
        Actor->AddInstanceComponent(Camera);
        Camera->SetupAttachment(Root);
        Camera->ProjectionType = ECameraProjectionMode::Orthographic;
        Camera->OrthoWidth = FullBodyOrthoWidth;
        Camera->bAutoCalculateOrthoPlanes = false;
        Camera->SetRelativeLocation(FVector(400, 0, FullBodyAimZ));
        Camera->SetRelativeRotation(FRotator(0, 180, 0));
        Camera->TextureTarget = ActiveRenderTarget;
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
        PreviewMaterial->SetTextureParameterValue(TEXT("PreviewTexture"), ActiveRenderTarget);
        Brush.SetResourceObject(PreviewMaterial.Get());
        if(bRoom)
        {
            TransitionMaterial.Reset(UMaterialInstanceDynamic::Create(Material,GetTransientPackage()));
            TransitionBrush.DrawAs=ESlateBrushDrawType::Image;
            TransitionBrush.ImageSize=FVector2D(1600,900);
            TransitionBrush.SetResourceObject(TransitionMaterial.Get());
        }
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
#if WITH_EDITOR
        const bool bMeasureSetup=bRoom && FParse::Param(FCommandLine::Get(),TEXT("RoomArrowCycleReview"));
        const double SetupStarted=bMeasureSetup?FPlatformTime::Seconds():0;
#endif
        const bool bSwitchingRoomCharacter=bRoom && ShownCharacter!=INDEX_NONE;
        SeniorWeaponPresentation::Destroy(HeldWeapon.Get()); HeldWeapon.Reset(); ShownWeapon=-1;
        if (Braxton.IsValid()) { Braxton->SetVisualActive(false); CachedBraxton=Braxton; Braxton.Reset(); }
        if (Runner.IsValid()) { Runner->SetVisualActive(false); CachedRunner=Runner; Runner.Reset(); }
        if (Fixer.IsValid()) { Fixer->SetVisualActive(false); CachedFixer=Fixer; Fixer.Reset(); }
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
        if (!Body)
        {
            UE_LOG(LogTemp, Error, TEXT("Senior character preview: mesh component is missing."));
            bNeedsCapture = true;
            return;
        }
        Body->SetVisibility(false);
        Capture->ShowFlags.SetDynamicShadows(Index == 0 || Index == 2);
        const FIntPoint DesiredSize = GetPreviewSize(Index);
        if (ActiveRenderTarget->SizeX != DesiredSize.X || ActiveRenderTarget->SizeY != DesiredSize.Y)
            ActiveRenderTarget->ResizeTarget(DesiredSize.X, DesiredSize.Y);
        bHasAnimation = false;
        if(!bSwitchingRoomCharacter)
        {
            RotationYaw = -8.0f;
            Zoom = 1.0f;
            PanOffset = 0;
        }
        TurnDemoStarted = FPlatformTime::Seconds();
        LastPrestreamTime = 0;
        // The original visual remains available as a recovery/debug option.
        if (Index == 0 && !FParse::Param(FCommandLine::Get(), TEXT("BraxtonLegacyVisual")))
        {
            ASeniorBraxtonVisual* NewVisual=CachedBraxton.Get();
            const bool bReused=NewVisual!=nullptr;
            if(bReused)CachedBraxton.Reset();
            else
            {
                FActorSpawnParameters Params;
                Params.ObjectFlags = RF_Transient;
                Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                NewVisual=PreviewWorld->SpawnActor<ASeniorBraxtonVisual>(PreviewActor->GetActorLocation(),FRotator::ZeroRotator,Params);
            }
            if (NewVisual && (bReused || NewVisual->InitializeVisual(true, false)))
            {
                Braxton = NewVisual;
                if(!bReused)NewVisual->AttachToActor(PreviewActor.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                NewVisual->SetActorRelativeRotation(FRotator(0,RotationYaw,0));
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
        // The original C02 remains available for visual regression checks.
        if (Index == 1 && !FParse::Param(FCommandLine::Get(), TEXT("RunnerLegacyVisual")))
        {
            ASeniorRunnerVisual* NewVisual=CachedRunner.Get();
            const bool bReused=NewVisual!=nullptr;
            if(bReused)CachedRunner.Reset();
            else
            {
                FActorSpawnParameters Params;
                Params.ObjectFlags = RF_Transient;
                Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                NewVisual=PreviewWorld->SpawnActor<ASeniorRunnerVisual>(PreviewActor->GetActorLocation(),FRotator::ZeroRotator,Params);
            }
            if (NewVisual && (bReused || NewVisual->InitializeVisual(true, false)))
            {
                Runner = NewVisual;
                if(!bReused)NewVisual->AttachToActor(PreviewActor.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                NewVisual->SetActorRelativeRotation(FRotator(0,RotationYaw,0));
                Body->SetVisibility(false);
                Capture->ShowOnlyActorComponents(NewVisual->GetNativeCharacter());
                Capture->ProjectionType = ECameraProjectionMode::Perspective;
                Capture->FOVAngle = 18.f;
                TInlineComponentArray<UPointLightComponent*> Lights(PreviewActor.Get());
                for (UPointLightComponent* Light : Lights)
                {
                    Light->SetIntensity(Light->Intensity * .28f);
                    Light->SetSourceRadius(12.f);
                    Light->SetSoftSourceRadius(22.f);
                }
                FaceAimZ = 177.f;
                bHasAnimation = false;
                NewVisual->KeepPreviewActive();
                UE_LOG(LogTemp, Display, TEXT("RUNNER_CHARACTER_PREVIEW_READY"));
            }
            else if (NewVisual) NewVisual->Destroy();
        }
        if (Index == 2 && !FParse::Param(FCommandLine::Get(), TEXT("FixerLegacyVisual")))
        {
            ASeniorFixerVisual* NewVisual=CachedFixer.Get();
            const bool bReused=NewVisual!=nullptr;
            if(bReused)CachedFixer.Reset();
            else
            {
                FActorSpawnParameters Params;
                Params.ObjectFlags = RF_Transient;
                Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                NewVisual=PreviewWorld->SpawnActor<ASeniorFixerVisual>(PreviewActor->GetActorLocation(),FRotator::ZeroRotator,Params);
            }
            if (NewVisual && (bReused || NewVisual->InitializeVisual(true, false)))
            {
                Fixer = NewVisual;
                if(!bReused)NewVisual->AttachToActor(PreviewActor.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                NewVisual->SetActorRelativeRotation(FRotator(0,RotationYaw,0));
                Body->SetVisibility(false);
                Capture->ShowOnlyActorComponents(NewVisual->GetNativeCharacter());
                Capture->ProjectionType = ECameraProjectionMode::Perspective;
                Capture->FOVAngle = 18.f;
                TInlineComponentArray<UPointLightComponent*> Lights(PreviewActor.Get());
                for (UPointLightComponent* Light : Lights)
                {
                    Light->SetIntensity(Light->Intensity * .28f);
                    Light->SetSourceRadius(12.f);
                    Light->SetSoftSourceRadius(22.f);
                }
                FaceAimZ = 156.f;
                bHasAnimation = false;
                NewVisual->KeepPreviewActive();
                UE_LOG(LogTemp, Display, TEXT("FIXER_CHARACTER_PREVIEW_READY"));
            }
            else if (NewVisual) NewVisual->Destroy();
        }
        // The native previews do not need their legacy Cobble
        // meshes built behind them. Only prepare that body for the remaining
        // characters or if a native visual could not be initialized.
        if(!Braxton.IsValid() && !Runner.IsValid() && !Fixer.IsValid())
        {
            if(USkeletalMesh* Asset=SeniorRoster::Body(Index))
            {
                Body->SetVisibility(true);
                Body->SetSkeletalMesh(Asset);
                Body->SetCastShadow(Index==0);
                UAnimSequence* Idle=SeniorRoster::Idle(Index);
                bHasAnimation=Idle!=nullptr;
                if(Idle)Body->PlayAnimation(Idle,true);
                Body->SetComponentTickEnabled(false);
                Body->TickAnimation(0,false);
                Body->RefreshBoneTransforms();
                const FBoxSphereBounds Bounds=Asset->GetBounds();
                const float Height=FMath::Max(float(Bounds.BoxExtent.Z*2.0),120.0f);
                const float LowestPoint=float(Bounds.Origin.Z-Bounds.BoxExtent.Z);
                Body->SetRelativeLocation(FVector(bRoom?40:0,0,-LowestPoint));
                FaceAimZ=Height-FMath::Clamp(Height*.075f,12.0f,15.0f);
                ClothMotion.Bind(Body,RotationYaw);
            }
            else UE_LOG(LogTemp,Error,TEXT("Senior character preview: body %d is missing."),Index+1);
        }
        // Assemble the detailed characters before the selection room
        // first appears. Arrow clicks can then reuse hidden previews instead of
        // synchronously building MetaHuman components in the middle of a browse.
        if(bRoom && !bSwitchingRoomCharacter)
        {
            FActorSpawnParameters Params;
            Params.ObjectFlags=RF_Transient;
            Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            if(!Braxton.IsValid() && !CachedBraxton.IsValid() && !FParse::Param(FCommandLine::Get(),TEXT("BraxtonLegacyVisual")))
            {
                if(auto* Ready=PreviewWorld->SpawnActor<ASeniorBraxtonVisual>(PreviewActor->GetActorLocation(),FRotator::ZeroRotator,Params))
                {
                    if(Ready->InitializeVisual(true,false))
                    {
                        Ready->AttachToActor(PreviewActor.Get(),FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                        Ready->SetVisualActive(false);
                        CachedBraxton=Ready;
                    }
                    else Ready->Destroy();
                }
            }
            if(!Runner.IsValid() && !CachedRunner.IsValid() && !FParse::Param(FCommandLine::Get(),TEXT("RunnerLegacyVisual")))
            {
                if(auto* Ready=PreviewWorld->SpawnActor<ASeniorRunnerVisual>(PreviewActor->GetActorLocation(),FRotator::ZeroRotator,Params))
                {
                    if(Ready->InitializeVisual(true,false))
                    {
                        Ready->AttachToActor(PreviewActor.Get(),FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                        Ready->SetVisualActive(false);
                        CachedRunner=Ready;
                    }
                    else Ready->Destroy();
                }
            }
            if(!Fixer.IsValid() && !CachedFixer.IsValid() && !FParse::Param(FCommandLine::Get(),TEXT("FixerLegacyVisual")))
            {
                if(auto* Ready=PreviewWorld->SpawnActor<ASeniorFixerVisual>(PreviewActor->GetActorLocation(),FRotator::ZeroRotator,Params))
                {
                    if(Ready->InitializeVisual(true,false))
                    {
                        Ready->AttachToActor(PreviewActor.Get(),FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                        Ready->SetVisualActive(false);
                        CachedFixer=Ready;
                    }
                    else Ready->Destroy();
                }
            }
        }
#if WITH_EDITOR
        if (bInteractive && FParse::Param(FCommandLine::Get(), TEXT("LobbyPreviewFace"))) Zoom = FaceZoom;
        FParse::Value(FCommandLine::Get(), TEXT("LobbyPreviewYaw="), RotationYaw);
        FParse::Value(FCommandLine::Get(), TEXT("LobbyPreviewZoom="), Zoom);
        FParse::Value(FCommandLine::Get(), TEXT("LobbyPreviewPan="), PanOffset);
#endif
        bNeedsCapture = true;
#if WITH_EDITOR
        if(bMeasureSetup)UE_LOG(LogTemp,Display,TEXT("CHARACTER_ROOM_SETUP_MS: index=%d duration=%.1f"),Index,(FPlatformTime::Seconds()-SetupStarted)*1000.0);
#endif
    }

    int32 HitRoomObject(const FGeometry& Geometry,FVector2D Screen) const
    {
        if(!bRoom || !Capture.IsValid())return -1;
        const FVector2D Local=Geometry.AbsoluteToLocal(Screen),Size=Geometry.GetLocalSize();
        if(Size.X<=0 || Size.Y<=0 || Local.X<0 || Local.Y<0 || Local.X>Size.X || Local.Y>Size.Y)return -1;
        const float Tan=FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle*.5f));
        const FVector Ray=Capture->GetComponentTransform().TransformVectorNoScale(FVector(1,(Local.X/Size.X*2-1)*Tan,(1-Local.Y/Size.Y*2)*Tan*Size.Y/Size.X)).GetSafeNormal();
        return Room.Pick(Capture->GetComponentLocation(),Ray);
    }

    void UpdateRoomHover(const FGeometry& Geometry,FVector2D Screen)
    {
        const int32 Hit=HitRoomObject(Geometry,Screen);
        Room.SetReturnHovered(Hit==2);
        Room.SetArrowHovered(Hit==3?0:Hit==4?1:-1);
        SetCursor(Hit>=0?EMouseCursor::Hand:EMouseCursor::GrabHand);
    }

    bool SelectRoomObject(const FGeometry& Geometry,FVector2D Screen)
    {
        const int32 Hit=HitRoomObject(Geometry,Screen);
        if(Hit==2){OnBack.ExecuteIfBound();return true;}
        if(Hit==3 || Hit==4){if(OnBrowse)OnBrowse(Hit==3?-1:1);return true;}
        if(Hit>=5 && Hit<5+SeniorRoster::Count)
        {
            const int32 Target=Hit-5,Current=CharacterIndex.Get();
            if(Target!=Current && OnBrowse)OnBrowse(Target-Current);
            return true;
        }
        if(Hit>=0 && Hit<2){if(OnWeapon)OnWeapon(Hit);return true;}
        return false;
    }
    int32 GetPreviewQuality() const
    {
        const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
        // OverallScalabilityLevel becomes "custom" when the independent 3D
        // render scale differs from a preset. Keep capture cost tied to the
        // actual shadow/effects quality rather than that resolution choice.
        const int32 Level = Settings ? FMath::Min(Settings->GetShadowQuality(),
            Settings->GetVisualEffectQuality()) : 2;
        return FMath::Clamp(Level, 0, 4);
    }
    FIntPoint GetPreviewSize(int32 Index) const
    {
        const int32 Quality = GetPreviewQuality();
        if (bRoom)
            return Quality >= 3 ? FIntPoint(1600, 900) :
                   Quality >= 2 ? FIntPoint(1280, 720) : FIntPoint(960, 540);
        const int32 Side = !bInteractive ? (Quality >= 2 ? 768 : 576) :
                           Index == 0 ? (Quality >= 3 ? 1280 : Quality >= 2 ? 1024 : 768) :
                                        (Quality >= 2 ? 1024 : 768);
        return FIntPoint(Side, Side);
    }
    FSeniorCharacterRoom Room;
    bool bRoom=false;
    bool bRoomValidated=false;
    int32 RoomReviewStage=0,RoomReviewOriginal=0;
    int32 RoomArrowReviewStage=0;
    double RoomArrowReviewStarted=0;
    int32 RoomRosterReviewStage=0;
    double RoomRosterReviewStarted=0;
    TFunction<void(int32)> OnWeapon;
    TFunction<void(int32)> OnBrowse;
    FSimpleDelegate OnBack;
    TWeakObjectPtr<UWorld> PreviewWorld;
    double TurnDemoStarted = 0;
    TWeakObjectPtr<AActor> PreviewActor;
    TWeakObjectPtr<ASeniorBraxtonVisual> Braxton;
    TWeakObjectPtr<ASeniorRunnerVisual> Runner;
    TWeakObjectPtr<ASeniorFixerVisual> Fixer;
    TWeakObjectPtr<ASeniorBraxtonVisual> CachedBraxton;
    TWeakObjectPtr<ASeniorRunnerVisual> CachedRunner;
    TWeakObjectPtr<ASeniorFixerVisual> CachedFixer;
    TWeakObjectPtr<USceneComponent> HeldWeapon;
    int32 ShownWeapon=-1;
    TWeakObjectPtr<USkeletalMeshComponent> Mesh;
    TWeakObjectPtr<USceneCaptureComponent2D> Capture;
    TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget;
    TStrongObjectPtr<UTextureRenderTarget2D> AlternateRenderTarget;
    TSharedPtr<FStreamableHandle> RosterPreload;
    TStrongObjectPtr<UMaterialInstanceDynamic> PreviewMaterial;
    TStrongObjectPtr<UMaterialInstanceDynamic> TransitionMaterial;
    UTextureRenderTarget2D* ActiveRenderTarget=nullptr;
    FSlateBrush TransitionBrush;
    double TransitionStarted=0;
    bool bStartTransitionAfterCapture=false;
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
TSharedRef<SWidget> MakeSeniorCharacterRoomPreview(UWorld* World,TAttribute<int32> CharacterIndex,TAttribute<int32> WeaponIndex,TFunction<void(int32)> OnWeapon,TFunction<void(int32)> OnBrowse,FSimpleDelegate OnBack)
{
    return SNew(SSeniorCharacterPreview).World(World).CharacterIndex(CharacterIndex).Interactive(true).WeaponIndex(WeaponIndex).Room(true).OnWeapon(OnWeapon).OnBrowse(OnBrowse).OnBack(OnBack);
}
