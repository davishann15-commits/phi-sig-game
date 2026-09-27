// Explicit integration modes use the isolated save slot selected by UStoryCampaign.
#include "SeniorLobby.h"
#include "SeniorCharacterRoster.h"
#include "SeniorBraxtonVisual.h"
#include "SeniorRunnerVisual.h"
#include "SeniorFixerVisual.h"
#include "SeniorLobbyAtmosphere.h"
#include "StoryCampaign.h"
#include "SeniorDouli.h"
#include "SeniorDouliAnim.h"
#include "SeniorDouliIdle.h"
#include "Components/StaticMeshComponent.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "ClothingSimulationInteractor.h"
#include "GroomComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

void TickSeniorLobbySmokeTest(ASeniorLobbyController* PC)
{
    FString Mode;
    if (!FParse::Value(FCommandLine::Get(), TEXT("LobbySmokeTest="), Mode)) return;
    if (Mode != TEXT("Solo") && Mode != TEXT("Resume") && Mode != TEXT("Host") && Mode != TEXT("Client") && Mode != TEXT("Visual") && Mode != TEXT("Hands") && Mode != TEXT("Dimensions") && Mode != TEXT("Atmosphere") && Mode != TEXT("PhotoMotion") && Mode != TEXT("Cloth")) return;
    static int32 Step = 0;
    static double Started = FPlatformTime::Seconds(), Last = 0, Changed = 0;
    auto Check = [](bool Condition, const TCHAR* Description)
    {
        if (!Condition)
        { UE_LOG(LogTemp, Error, TEXT("LOBBY_TEST_FAILED: %s"), Description); FPlatformMisc::RequestExitWithStatus(false, 1); }
        return Condition;
    };
    auto CheckCharacterAppearance = [&Check](AStoryFirstPersonCharacter* Pawn)
    {
        const auto* Selected = Pawn ? Pawn->GetPlayerState<ASeniorLobbyPlayerState>() : nullptr;
        if (!Check(Selected != nullptr, TEXT("Character appearance has no selected PlayerState"))) return false;
        USkeletalMesh* ExpectedBody = SeniorRoster::Body(Selected->CharacterIndex);
        USkeletalMesh* ExpectedArms = SeniorRoster::Arms(Selected->CharacterIndex);
        if (!Check(ExpectedBody && ExpectedArms, TEXT("Selected character body or arms asset is missing"))) return false;
        if (!Check(Pawn->GetMesh()->GetSkeletalMeshAsset() == ExpectedBody
            && Pawn->FirstPersonArms && Pawn->FirstPersonArms->GetSkeletalMeshAsset() == ExpectedArms,
            TEXT("Selected body and first-person arms were not applied after travel"))) return false;
        if (!Check(Pawn->GetMesh()->bOwnerNoSee && !Pawn->GetMesh()->bOnlyOwnerSee
            && Pawn->FirstPersonArms->bOnlyOwnerSee && !Pawn->FirstPersonArms->bOwnerNoSee
            && !Pawn->FirstPersonArms->bHiddenInGame
            && Pawn->FirstPersonArms->GetAttachParent() == Pawn->FirstPersonCamera,
            TEXT("First-person arms or teammate body visibility configuration is wrong"))) return false;
        UE_LOG(LogTemp, Display, TEXT("LOBBY_TEST: Character %d body and owner-only arms verified"), Selected->CharacterIndex);
        if (Selected->CharacterIndex == 0 && Pawn->GetNetMode() != NM_DedicatedServer
            && !FParse::Param(FCommandLine::Get(), TEXT("BraxtonLegacyVisual")))
        {
            int32 NativeCount = 0;
            for (ASeniorBraxtonVisual* Visual : TActorRange<ASeniorBraxtonVisual>(Pawn->GetWorld()))
                if (Visual->GetOwner() == Pawn)
                {
                    ++NativeCount;
                    AActor* Native = Visual->GetNativeCharacter();
                    if (!Check(Native && !Native->IsHidden() && Native->GetOwner() == Pawn,
                        TEXT("Native Braxton body is missing, hidden or has the wrong owner"))) return false;
                    // Groom setup creates non-rendering Niagara simulation
                    // primitives later. Assert visibility on actual visual meshes.
                    TInlineComponentArray<UMeshComponent*> Parts(Native);
                    for (const UMeshComponent* Part : Parts)
                        if (!Check(Part->bOwnerNoSee && !Part->bOnlyOwnerSee,
                            TEXT("Native body must be hidden from its first-person owner but visible to teammates"))) return false;
                }
            if (!Check(NativeCount == 1 && !Pawn->GetMesh()->IsVisible(),
                TEXT("Braxton needs exactly one native replacement and no visible legacy body"))) return false;
            UE_LOG(LogTemp,Display,TEXT("LOBBY_TEST: Native Braxton replacement and teammate visibility verified"));
        }
        if (Selected->CharacterIndex == 1 && Pawn->GetNetMode() != NM_DedicatedServer
            && !FParse::Param(FCommandLine::Get(), TEXT("RunnerLegacyVisual")))
        {
            int32 NativeCount = 0;
            for (ASeniorRunnerVisual* Visual : TActorRange<ASeniorRunnerVisual>(Pawn->GetWorld()))
                if (Visual->GetOwner() == Pawn)
                {
                    ++NativeCount;
                    AActor* Native = Visual->GetNativeCharacter();
                    if (!Check(Native && !Native->IsHidden() && Native->GetOwner() == Pawn,
                        TEXT("Native Runner body is missing, hidden or has the wrong owner"))) return false;
                    TInlineComponentArray<UMeshComponent*> Parts(Native);
                    for (const UMeshComponent* Part : Parts)
                        if (!Check(Part->bOwnerNoSee && !Part->bOnlyOwnerSee,
                            TEXT("Native Runner must be hidden from its first-person owner but visible to teammates"))) return false;
                }
            if (!Check(NativeCount == 1 && !Pawn->GetMesh()->IsVisible(),
                TEXT("Runner needs exactly one native replacement and no visible legacy body"))) return false;
            UE_LOG(LogTemp, Display, TEXT("LOBBY_TEST: Native Runner replacement and teammate visibility verified"));
        }
        if (Selected->CharacterIndex == 2 && Pawn->GetNetMode() != NM_DedicatedServer
            && !FParse::Param(FCommandLine::Get(), TEXT("FixerLegacyVisual")))
        {
            int32 NativeCount = 0;
            for (ASeniorFixerVisual* Visual : TActorRange<ASeniorFixerVisual>(Pawn->GetWorld()))
                if (Visual->GetOwner() == Pawn)
                {
                    ++NativeCount;
                    AActor* Native = Visual->GetNativeCharacter();
                    if (!Check(Native && !Native->IsHidden() && Native->GetOwner() == Pawn,
                        TEXT("Native Fixer body is missing, hidden or has the wrong owner"))) return false;
                    if (!Check(Visual->GetBodyMesh() && Visual->GetIdleAnimation() && Visual->GetWalkAnimation(),
                        TEXT("Native Fixer body or movement animation is missing"))) return false;
                    TInlineComponentArray<UMeshComponent*> Parts(Native);
                    for (const UMeshComponent* Part : Parts)
                        if (!Check(Part->bOwnerNoSee && !Part->bOnlyOwnerSee,
                            TEXT("Native Fixer must be hidden from its first-person owner but visible to teammates"))) return false;
                }
            if (!Check(NativeCount == 1 && !Pawn->GetMesh()->IsVisible(),
                TEXT("Fixer needs exactly one native replacement and no visible legacy body"))) return false;
            UE_LOG(LogTemp, Display, TEXT("LOBBY_TEST: Native Fixer replacement and teammate visibility verified"));
        }
        return true;
    };
    if (Mode == TEXT("Cloth"))
    {
        auto* TestMesh = NewObject<USkeletalMeshComponent>(PC);
        TestMesh->SetSkeletalMeshAsset(SeniorRoster::Body(0));
        FSeniorClothMotion Motion; Motion.Bind(TestMesh, 0);
        if (!Check(!Motion.Left.IsEmpty() && !Motion.Right.IsEmpty() && !Motion.Breath.IsEmpty(), TEXT("Braxton cloth shapes were not imported"))) return;
        for (const UMorphTarget* Morph : TestMesh->GetSkeletalMeshAsset()->GetMorphTargets())
        {
            if (!Morph || !Morph->GetName().StartsWith(TEXT("Cloth"))) continue;
            for (int32 LOD = 0; LOD < 3; ++LOD)
            {
                UE_LOG(LogTemp, Display, TEXT("BRAXTON_CLOTH_DATA: %s LOD=%d deltas=%d"), *Morph->GetName(), LOD, Morph->GetNumDeltasForLOD(LOD));
                if (!Check(Morph->HasDataForLOD(LOD), TEXT("Cloth deformation missing from a distance LOD"))) return;
            }
        }
        float Reference = 0;
        for (int32 FPS : {30, 60, 120})
        {
            Motion.Bind(TestMesh, 0);
            for (int32 I = 1; I <= FPS; ++I) Motion.Update(TestMesh, 1.f / FPS, I * 150.f / FPS);
            if (!Check(Motion.Position < -.2f && FMath::Abs(Motion.Position) <= 1.f && TestMesh->GetMorphTarget(Motion.Right[0]) > .2f,
                TEXT("Turning must move the real cloth target in the opposite direction"))) return;
            if (FPS == 30) Reference = Motion.Position;
            if (!Check(FMath::Abs(Motion.Position - Reference) < .035f, TEXT("Cloth response changes too much with frame rate"))) return;
            for (int32 I = 0; I < FPS * 4; ++I) Motion.Update(TestMesh, 1.f / FPS, 150.f);
            if (!Check(FMath::Abs(Motion.Position) < .001f && FMath::Abs(Motion.Velocity) < .001f, TEXT("Cloth failed to settle after rotation stopped"))) return;
        }
        Motion.Bind(TestMesh, 179.f); Motion.Update(TestMesh, 1.f / 60.f, -179.f);
        if (!Check(FMath::Abs(Motion.Position) < .02f, TEXT("Yaw wrap produced a cloth spike"))) return;
        Motion.Update(TestMesh, 2.f, 50.f);
        if (!Check(Motion.Position == 0 && Motion.Velocity == 0, TEXT("Hidden preview resume must reset cloth inertia"))) return;
        UE_LOG(LogTemp, Display, TEXT("LOBBY_TEST_PASSED: Cloth imported, turns, settles, frame-rate stability and yaw-wrap/hitch safety"));
        FPlatformMisc::RequestExitWithStatus(false, 0); return;
    }
    const double Now = FPlatformTime::Seconds();
    auto RenderAssetsReady = [Now]()
    {
        static double LastBusy = Now;
        if (FAssetCompilingManager::Get().GetNumRemainingAssets() > 0
            || (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()))
        {
            LastBusy = Now;
            return false;
        }
        return Now - LastBusy > 1.0;
    };
    if (!Check(Now - Started < 180, TEXT("Lobby test timed out")) || Now - Last < (Mode == TEXT("PhotoMotion") ? .016 : .25)) return;
    Last = Now;
    UWorld* World = PC->GetWorld();
    auto* State = World->GetGameState<ASeniorLobbyGameState>();
    auto* Player = PC->GetPlayerState<ASeniorLobbyPlayerState>();
    auto* Story = PC->GetGameInstance<UStoryCampaign>();
    if (!State || !Player || !Story) return;
    const FString Map = UGameplayStatics::GetCurrentLevelName(World, true);
    const bool Lobby = Map == TEXT("Lobby");
    static TMap<TWeakObjectPtr<ASeniorDouli>,uint8> DouliSeen;
    static double DouliChapterStart = 0;
    static bool bDouliSent = false;
    static bool bDouliNetworkPassed = false;
    const bool bDouliNetworkTest=FParse::Param(FCommandLine::Get(),TEXT("DouliNetworkTest"));
    if (bDouliNetworkTest && Map==TEXT("Chapter01"))
    {
        if(DouliChapterStart==0)DouliChapterStart=Now;
        PC->SetControlRotation(FRotator(55,0,0));
        auto* P=Cast<AStoryFirstPersonCharacter>(PC->GetPawn());
        for(ASeniorDouli* Weapon : TActorRange<ASeniorDouli>(World))
            DouliSeen.FindOrAdd(Weapon)|=uint8(1<<int32(Weapon->Phase));
        if(P && P->Douli && !bDouliSent && Now-DouliChapterStart>4)
        { P->ServerThrowDouli();bDouliSent=true; }
        if(Now-DouliChapterStart>11 && !bDouliNetworkPassed)
        {
            int32 Good=0;
            const uint8 Required=(1<<int32(EDouliPhase::Ready))|(1<<int32(EDouliPhase::Outbound))|(1<<int32(EDouliPhase::Returning));
            for(const auto& Seen:DouliSeen) if((Seen.Value&Required)==Required)++Good;
            if(!Check(Good==3,TEXT("Every peer must observe all three hats ready, outbound and returning")))return;
            UE_LOG(LogTemp,Display,TEXT("DOULI_NETWORK_PASSED: all three replicated throw/return cycles observed"));
            bDouliNetworkPassed=true;
        }
    }
    if (Mode == TEXT("PhotoMotion"))
    {
        if (!Lobby || !RenderAssetsReady()) return;
        const auto Atmosphere = GetSeniorLobbyAtmosphereDiagnostics();
        static int32 Frame = 0;
        static bool bRequested = false;
        static uint64 PriorPaints = 0;
        float PreviewStart=20.f;
        FParse::Value(FCommandLine::Get(),TEXT("LobbyPhotoStart="),PreviewStart);
        const FString Directory = FPaths::ProjectSavedDir() / (PreviewStart>=50.f
            ? TEXT("Screenshots/PhotoLeavesHuge") : TEXT("Screenshots/PhotoLeavesNormal"));
        const FString File = Directory / FString::Printf(TEXT("Frame%03d.png"), Frame);
        if (Step == 0)
        {
            if (!Check(Atmosphere.bSkyMaterial, TEXT("Photo motion material did not load"))) return;
            IFileManager::Get().MakeDirectory(*Directory, true);
            SetSeniorLobbyPhotoPreviewTime(PreviewStart);
            PriorPaints = Atmosphere.Paints; Step = 1;
            return;
        }
        if (bRequested)
        {
            if (FScreenshotRequest::IsScreenshotRequested() || !FPaths::FileExists(File)) return;
            ++Frame; bRequested = false;
            if (Frame == 240)
            {
                SetSeniorLobbyPhotoPreviewTime(TOptional<double>());
                UE_LOG(LogTemp, Display, TEXT("LOBBY_TEST_PASSED: 240 actual Unreal photo-motion frames captured at 15 FPS"));
                FPlatformMisc::RequestExitWithStatus(false, 0);
                return;
            }
            SetSeniorLobbyPhotoPreviewTime(PreviewStart + Frame / 15.0);
            PriorPaints = Atmosphere.Paints;
            return;
        }
        if (Atmosphere.Paints > PriorPaints + 2 && FMath::Abs(Atmosphere.Seconds - (PreviewStart + Frame / 15.0)) < .001)
        {
            FScreenshotRequest::RequestScreenshot(File, true, false);
            bRequested = true;
            if (Frame % 30 == 0) UE_LOG(LogTemp, Display, TEXT("LOBBY_PHOTO_FRAME: %d time=%.3f"), Frame, Atmosphere.Seconds);
        }
        return;
    }
    if (Mode == TEXT("Atmosphere"))
    {
        if (!Lobby || !RenderAssetsReady()) return;
        const auto Atmosphere = GetSeniorLobbyAtmosphereDiagnostics();
        if (!Check(Atmosphere.bSkyMaterial && Atmosphere.Leaves >= 200 && (Step == 1 || Atmosphere.bAnimating),
            TEXT("Animated sky, leaves, or visible lobby ticking is missing"))) return;
        static double FirstSceneTime = 0;
        static uint64 FirstPaints = 0;
        static int32 Captures = 0;
        static double FrozenSceneTime = 0;
        // Capture an actual timed passage, not a frozen diagnostic override.
        if (Captures == 0 && Atmosphere.WindowPassers > 0 && Atmosphere.Seconds > 8.5)
        {
            FirstSceneTime = Atmosphere.Seconds;
            FirstPaints = Atmosphere.Paints;
        }
        const bool CaptureNow = (Captures == 0 && FirstSceneTime > 0)
            || (Captures == 1 && Atmosphere.Seconds > FirstSceneTime + 2.5)
            || (Captures == 2 && Atmosphere.Seconds > FirstSceneTime + 8);
        if (CaptureNow && !FScreenshotRequest::IsScreenshotRequested())
        {
            const FString File = FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/LobbyAtmosphere_%d.png"), Captures + 1);
            FScreenshotRequest::RequestScreenshot(File, true, false);
            UE_LOG(LogTemp, Display, TEXT("LOBBY_ATMOSPHERE_FRAME: %d seconds=%.3f leaves=%d windowPassers=%d paints=%llu"),
                Captures + 1, Atmosphere.Seconds, Atmosphere.Leaves, Atmosphere.WindowPassers, Atmosphere.Paints);
            ++Captures; Changed = Now;
        }
        if (Captures == 3 && Now - Changed > 2 && !FScreenshotRequest::IsScreenshotRequested())
        {
            if (Step == 1)
            {
                if (!Check(!Atmosphere.bAnimating && FMath::Abs(Atmosphere.Seconds - FrozenSceneTime) < .15,
                    TEXT("Reduced motion failed to freeze lobby animation"))) return;
                SetSeniorLobbyMotionTestOverride(true);
                Changed = Now; Step = 2;
                return;
            }
            if (Step == 2)
            {
                if (!Check(Atmosphere.bAnimating && Atmosphere.Seconds > FrozenSceneTime + .7,
                    TEXT("Lobby animation failed to resume"))) return;
                SetSeniorLobbyMotionTestOverride(TOptional<bool>());
                UE_LOG(LogTemp, Display, TEXT("LOBBY_TEST_PASSED: Animated sky, original-photo leaves, timed window passage, live captures, reduced-motion freeze and resume"));
                FPlatformMisc::RequestExitWithStatus(false, 0);
                return;
            }
            for (int32 Index = 1; Index <= 3; ++Index)
                if (!Check(FPaths::FileExists(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/LobbyAtmosphere_%d.png"), Index)),
                    TEXT("Timed atmosphere screenshot missing"))) return;
            if (!Check(Atmosphere.Paints > FirstPaints + 20 && Atmosphere.Seconds > FirstSceneTime + 8,
                TEXT("Lobby atmosphere did not animate over time"))) return;
            FrozenSceneTime = Atmosphere.Seconds;
            SetSeniorLobbyMotionTestOverride(false);
            Changed = Now; Step = 1;
        }
        return;
    }
    if (Mode == TEXT("Dimensions"))
    {
        auto* Pawn = Cast<AStoryFirstPersonCharacter>(PC->GetPawn());
        if (Lobby || !Pawn || !Pawn->HasAuthority() || Pawn->GetPlayerState() != Player || Step != 0) return;
        const float WalkSpeed = Pawn->GetCharacterMovement()->MaxWalkSpeed;
        const float JumpSpeed = Pawn->GetCharacterMovement()->JumpZVelocity;
        const float Radius = Pawn->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
        float ShortHeight = MAX_flt, TallHeight = 0, ShortEye = 0, TallEye = 0;
        for (int32 Index = 0; Index < SeniorRoster::Count; ++Index)
        {
            const float FeetBefore = Pawn->GetActorLocation().Z - Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            Player->CharacterIndex = Index;
            Player->OnRep_CharacterIndex();
            if (!CheckCharacterAppearance(Pawn)) return;
            const FBoxSphereBounds Bounds = Pawn->GetMesh()->GetSkeletalMeshAsset()->GetBounds();
            const bool bNativeFixer = Index == 2 && !FParse::Param(FCommandLine::Get(), TEXT("FixerLegacyVisual"));
            const float Height = bNativeFixer ? ASeniorFixerVisual::HeightCm : float(Bounds.BoxExtent.Z * 2.0);
            const float LowestPoint = bNativeFixer ? 0.f : float(Bounds.Origin.Z - Bounds.BoxExtent.Z);
            const float HalfHeight = Pawn->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
            const float FeetAfter = Pawn->GetActorLocation().Z - Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            const float EyeHeight = Pawn->FirstPersonCamera->GetRelativeLocation().Z + HalfHeight;
            if (!Check(FMath::IsNearlyEqual(Height, HalfHeight * 2, .25f)
                && FMath::IsNearlyEqual(float(FeetBefore), FeetAfter, .25f), TEXT("Stature update moved the feet or mismatched the collision height"))) return;
            if (!Check(FMath::IsNearlyEqual(float(Pawn->GetMesh()->GetRelativeLocation().Z + LowestPoint), -HalfHeight, .25f)
                && FMath::IsNearlyEqual(EyeHeight, Height * .935f, .25f), TEXT("Mesh floor or eye level does not match character height"))) return;
            if (!Check(Pawn->GetBaseTranslationOffset().Equals(Pawn->GetMesh()->GetRelativeLocation(), .1f),
                TEXT("Network smoothing still uses a different character's mesh offset"))) return;
            if (!Check(Pawn->FirstPersonArms->GetRelativeLocation().Equals(Pawn->ArmsRelativeTransform.GetTranslation() * (Height / 184.f), .1f)
                && Pawn->GetMesh()->GetRelativeScale3D().Equals(FVector::OneVector, .001f), TEXT("Body or first-person arms were scaled twice"))) return;
            if (!Check(FMath::IsNearlyEqual(Radius, Pawn->GetCapsuleComponent()->GetUnscaledCapsuleRadius())
                && FMath::IsNearlyEqual(WalkSpeed, Pawn->GetCharacterMovement()->MaxWalkSpeed)
                && FMath::IsNearlyEqual(JumpSpeed, Pawn->GetCharacterMovement()->JumpZVelocity), TEXT("Stature update changed movement balance"))) return;
            if (Height > TallHeight) { TallHeight = Height; TallEye = EyeHeight; }
            if (Height < ShortHeight) { ShortHeight = Height; ShortEye = EyeHeight; }
            UE_LOG(LogTemp, Display, TEXT("CHARACTER_DIMENSIONS: C%02d height=%.2f eye=%.2f half=%.2f"), Index + 1, Height, EyeHeight, HalfHeight);
        }
        if (!Check(TallHeight - ShortHeight >= 12.f && TallEye - ShortEye >= 11.f,
            TEXT("Shortest and tallest character assets do not have the intended visible height separation"))) return;
        UE_LOG(LogTemp, Display, TEXT("LOBBY_TEST_PASSED: All eight character dimensions, fixed feet, proportional arms, and movement settings"));
        FPlatformMisc::RequestExitWithStatus(false, 0); Step = 5;
        return;
    }
    if (Mode == TEXT("Hands"))
    {
        int32 Character = 0;
        FParse::Value(FCommandLine::Get(), TEXT("LobbyCharacter="), Character);
        if (!Check(SeniorRoster::IsValidIndex(Character), TEXT("Hands preview character must be in range 0..7"))) return;
        if (!Check(World->GetNetMode() == NM_Standalone, TEXT("Hands preview requires a standalone chapter"))) return;
        auto* CharacterPawn = Cast<AStoryFirstPersonCharacter>(PC->GetPawn());
        if (Lobby || !CharacterPawn || CharacterPawn->GetPlayerState() != Player) return;
        if (Step == 0)
        {
            Player->CharacterIndex = Character;
            Player->OnRep_CharacterIndex();
            PC->SetControlRotation(FRotator::ZeroRotator);
            if (!CheckCharacterAppearance(CharacterPawn)) return;
            Step = 1; Changed = Now;
        }
        else if (Step == 1 && Now - Changed > 5 && RenderAssetsReady())
        {
            const FString Screenshot = FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/CobbleHands_C%02d.png"), Character + 1);
            FScreenshotRequest::RequestScreenshot(Screenshot, true, false);
            UE_LOG(LogTemp, Display, TEXT("LOBBY_HANDS: Capturing character %d at a level first-person camera"), Character);
            Step = 2; Changed = Now;
        }
        else if (Step == 2 && Now - Changed > 3 && !FScreenshotRequest::IsScreenshotRequested())
        {
            const FString Screenshot = FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/CobbleHands_C%02d.png"), Character + 1);
            if (!Check(FPaths::FileExists(Screenshot), TEXT("First-person hands screenshot was not written"))) return;
            UE_LOG(LogTemp, Display, TEXT("LOBBY_TEST_PASSED: Character %d first-person appearance and screenshot"), Character);
            FPlatformMisc::RequestExitWithStatus(false, 0); Step = 5;
        }
        return;
    }
    if (Mode == TEXT("Visual"))
    {
        FString RequestedPage, PreviewPage = TEXT("Lobby");
        FParse::Value(FCommandLine::Get(), TEXT("LobbyPreviewPage="), RequestedPage);
        for (const TCHAR* AllowedPage : { TEXT("Lobby"), TEXT("Character"), TEXT("Loadout"), TEXT("Settings"), TEXT("Achievements"), TEXT("News"), TEXT("Credits"), TEXT("MainLobby") })
            if (RequestedPage.Equals(AllowedPage, ESearchCase::IgnoreCase)) { PreviewPage = AllowedPage; break; }

        static bool bPreviewFixturesInitialized = false;
        if (!bPreviewFixturesInitialized && Lobby)
        {
            bPreviewFixturesInitialized = true;
            int32 PreviewPlayers = 1;
            FParse::Value(FCommandLine::Get(), TEXT("LobbyPreviewPlayers="), PreviewPlayers);
            TArray<int32> PreviewCharacters = {0, 1, 2};
            FString CharacterList;
            if (FParse::Value(FCommandLine::Get(), TEXT("LobbyPreviewCharacters="), CharacterList, false))
            {
                TArray<FString> Tokens;
                CharacterList.ParseIntoArray(Tokens, TEXT(","), true);
                if (!Check(Tokens.Num() == 3, TEXT("Height preview requires three comma-separated roster indices"))) return;
                for (int32 Index = 0; Index < 3; ++Index)
                {
                    PreviewCharacters[Index] = FCString::Atoi(*Tokens[Index]);
                    if (!Check(SeniorRoster::IsValidIndex(PreviewCharacters[Index]), TEXT("Invalid height-preview character"))) return;
                }
            }
            // These fixtures exist only in this explicit editor screenshot mode, never in a LAN session or packaged game.
            if (PreviewPlayers == 3 && World->GetNetMode() == NM_Standalone && PC->HasAuthority() && State->GetMembers().Num() == 1)
            {
                Player->CharacterIndex = PreviewCharacters[0]; Player->LoadoutIndex = 0; Player->bReady = true;
                Player->SetPlayerName(TEXT("Player 1")); Player->ForceNetUpdate();
                for (int32 Index = 1; Index < 3; ++Index)
                {
                    auto* Fixture = World->SpawnActor<ASeniorLobbyPlayerState>();
                    if (!Check(Fixture != nullptr, TEXT("Could not create visual-only player fixture"))) return;
                    Fixture->SetReplicates(true);
                    Fixture->SetPlayerId(Player->GetPlayerId() + Index);
                    Fixture->SetPlayerName(FString::Printf(TEXT("Player %d"), Index + 1));
                    Fixture->CharacterIndex = PreviewCharacters[Index]; Fixture->LoadoutIndex = Index % 2;
                    Fixture->bReady = true; Fixture->bIsHost = false;
                    State->AddPlayerState(Fixture); Fixture->ForceNetUpdate();
                }
                UE_LOG(LogTemp, Display, TEXT("LOBBY_VISUAL: Created two temporary screenshot player fixtures"));
            }
        }
        if (FParse::Param(FCommandLine::Get(),TEXT("DouliIdleReview")) && RenderAssetsReady())
        {
            static double ReviewStart=Now;
            static int32 Frame=0;
            static float MinHeight=10000, MaxHeight=-10000, MaxGripError=0, MinLeftClearance=10000;
            static float MaxFingerGap=0, MinTorsoClearance=10000;
            static float MinHeadShellDistance=10000;
            static float MinTorsoTime=0;
            static FVector PreviousElbows[2]={FVector::ZeroVector,FVector::ZeroVector};
            static float PreviousElbowTime=-1,MaxElbowStep=0;
            static bool SawAir=false,SawHead=false;
            static int32 PassFlights=0,LeftCatches=0;
            static FSeniorDouliIdle::EAttachment PreviousAttachment=FSeniorDouliIdle::EAttachment::Hand;
            static float MaxAirSeparation=0,MaxHeadHandSeparation=0,LastCycleTime=0;
            for (ASeniorBraxtonVisual* V:TActorRange<ASeniorBraxtonVisual>(World))
            {
                auto* B=V->GetBodyMesh();
                if(!B || !V->GetNativeCharacter())continue;
                auto* Anim=Cast<USeniorDouliAnim>(B->GetAnimInstance());
                if(!Anim || !Anim->bLobbyIdle || !Anim->bEquipped)continue;
                const FSeniorDouliIdle Cycle=FSeniorDouliIdle::At(Anim->LobbyTime);
                if(Cycle.Attachment!=PreviousAttachment)
                {
                    if(Anim->LobbyTime>=32 && Cycle.Attachment==FSeniorDouliIdle::EAttachment::Air)++PassFlights;
                    if(Cycle.Attachment==FSeniorDouliIdle::EAttachment::LeftHand)++LeftCatches;
                    PreviousAttachment=Cycle.Attachment;
                }
                LastCycleTime=Anim->LobbyTime;
                const float ElbowDt=LastCycleTime-PreviousElbowTime;
                for(int32 Arm=0;Arm<2;++Arm)
                {
                    const FVector Elbow=B->GetComponentTransform().InverseTransformPosition(B->GetSocketLocation(Arm==0?TEXT("lowerarm_r"):TEXT("lowerarm_l")));
                    if(PreviousElbowTime>=0 && ElbowDt>0 && ElbowDt<.06f)
                        MaxElbowStep=FMath::Max(MaxElbowStep,float(FVector::Dist(Elbow,PreviousElbows[Arm])));
                    PreviousElbows[Arm]=Elbow;
                }
                PreviousElbowTime=LastCycleTime;
                TInlineComponentArray<UStaticMeshComponent*> Props(V->GetNativeCharacter());
                for(auto* Hat:Props) if(Hat->GetFName()==TEXT("LobbyDouli"))
                {
                    const bool Right=Cycle.Attachment!=FSeniorDouliIdle::EAttachment::LeftHand;
                    const FVector Grip=Hat->GetComponentTransform().TransformPosition(-FSeniorDouliIdle::HatOffset(Right));
                    if(Cycle.Attachment==FSeniorDouliIdle::EAttachment::Hand || Cycle.Attachment==FSeniorDouliIdle::EAttachment::LeftHand)
                    {
                        MaxGripError=FMath::Max(MaxGripError,float(FVector::Dist(Grip,B->GetSocketLocation(Right?TEXT("hand_r"):TEXT("hand_l")))));
                        const FVector Rim=Hat->GetComponentTransform().TransformPosition(FVector(-20,0,0));
                        MaxFingerGap=FMath::Max(MaxFingerGap,float(FVector::Dist(Rim,B->GetSocketLocation(Right?TEXT("middle_03_r"):TEXT("middle_03_l")))));
                        MinLeftClearance=FMath::Min(MinLeftClearance,float(FVector::Dist(Hat->GetComponentLocation(),B->GetSocketLocation(Right?TEXT("hand_l"):TEXT("hand_r")))));
                    }
                    else if(Cycle.Attachment==FSeniorDouliIdle::EAttachment::Air)
                    {
                        SawAir=true;
                        MaxAirSeparation=FMath::Max(MaxAirSeparation,float(FVector::Dist(Grip,B->GetSocketLocation(TEXT("hand_r")))));
                    }
                    else
                    {
                        SawHead=true;
                        MaxHeadHandSeparation=FMath::Max(MaxHeadHandSeparation,float(FVector::Dist(Hat->GetComponentLocation(),B->GetSocketLocation(TEXT("hand_r")))));
                    }
                    // Sample the actual conical shell against a padded torso volume,
                    // rather than testing only the prop's centre or wrist attachment.
                    const FBox Torso(FVector(-23,-17,78),FVector(23,17,147));
                    for(int32 Ring=0;Ring<=4;++Ring)for(int32 Segment=0;Segment<32;++Segment)
                    {
                        const float Radius=Ring*5.f, Angle=Segment*2.f*PI/32.f;
                        const FVector Surface(Radius*FMath::Cos(Angle),Radius*FMath::Sin(Angle),14.f*(1.f-FMath::Pow(Radius/20.f,.96f)));
                        const FVector P=B->GetComponentTransform().InverseTransformPosition(Hat->GetComponentTransform().TransformPosition(Surface));
                        const float Clearance=float(FVector::Dist(P,Torso.GetClosestPointTo(P)));
                        const FVector HeadCentre=B->GetComponentTransform().InverseTransformPosition(B->GetSocketLocation(TEXT("head")))+FVector(0,0,8);
                        const FVector HeadDelta=(P-HeadCentre)/FVector(8.f,9.f,10.f);
                        MinHeadShellDistance=FMath::Min(MinHeadShellDistance,float(HeadDelta.Size()));
                        if(Clearance<MinTorsoClearance) { MinTorsoClearance=Clearance; MinTorsoTime=Anim->LobbyTime; }
                    }
                    const float Height=B->GetComponentTransform().InverseTransformPosition(Hat->GetComponentLocation()).Z;
                    MinHeight=FMath::Min(MinHeight,Height);
                    MaxHeight=FMath::Max(MaxHeight,Height);
                    const float CaptureTimes[]={1.f,6.30f,15.5f,17.8f,20.7f,24.5f,32.22f,32.65f,33.32f,34.9f,38.5f};
                    if(Frame<11 && Anim->LobbyTime>=CaptureTimes[Frame])
                    {
                        if(Frame==0)for(const TCHAR* BoneName:{TEXT("index_03_r"),TEXT("middle_03_r"),TEXT("thumb_03_r")})
                            UE_LOG(LogTemp,Display,TEXT("DOULI_GRIP_BONE %s hatLocal=%s"),BoneName,*Hat->GetComponentTransform().InverseTransformPosition(B->GetSocketLocation(BoneName)).ToString());
                        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/DouliIdle_%d.png"),Frame),true,false);
                        UE_LOG(LogTemp,Display,TEXT("DOULI_IDLE_FRAME %d time=%.2f gripError=%.2f leftClearance=%.2f fingerGap=%.2f torsoClearance=%.2f"),Frame,Anim->LobbyTime,MaxGripError,MinLeftClearance,MaxFingerGap,MinTorsoClearance);
                        ++Frame;
                    }
                }
            }
            if(LastCycleTime>41.f)
            {
                UE_LOG(LogTemp,Display,TEXT("DOULI_CYCLE_METRICS: frames=%d air=%d head=%d heightRange=%.2f gripError=%.2f left=%.2f fingers=%.2f torso=%.2f airborneGap=%.2f wornGap=%.2f"),Frame,SawAir,SawHead,MaxHeight-MinHeight,MaxGripError,MinLeftClearance,MaxFingerGap,MinTorsoClearance,MaxAirSeparation,MaxHeadHandSeparation);
                UE_LOG(LogTemp,Display,TEXT("DOULI_PASS_METRICS: flights=%d leftCatches=%d minTorsoTime=%.2f"),PassFlights,LeftCatches,MinTorsoTime);
                UE_LOG(LogTemp,Display,TEXT("DOULI_ELBOW_METRICS: maxFrameStepCm=%.2f"),MaxElbowStep);
                UE_LOG(LogTemp,Display,TEXT("DOULI_HEAD_CLEARANCE: shellEllipsoidDistance=%.3f"),MinHeadShellDistance);
                if(!Check(MinHeadShellDistance>1.f,TEXT("Hat shell entered the head safety volume")))return;
                if(!Check(MaxElbowStep<12.f,TEXT("Lobby elbow snapped between adjacent frames")))return;
                if(!Check(Frame==11 && PassFlights==4 && LeftCatches==2 && SawAir && SawHead && MaxHeight-MinHeight>90 && MaxGripError<4 && MinLeftClearance>27 && MaxFingerGap<6 && MinTorsoClearance>3 && MaxAirSeparation>35 && MaxHeadHandSeparation>45,
                    TEXT("Cycle must relax, release/catch, wear/remove, preserve grip and stay clear of the body")))return;
                UE_LOG(LogTemp,Display,TEXT("DOULI_IDLE_PASSED: relaxed hold, toss/catch, head wear/removal, four hand-to-hand passes and mirrored left grip"));
                FPlatformMisc::RequestExitWithStatus(false,0);
            }
            if(Now-ReviewStart>120) Check(false,TEXT("Lobby choreography did not finish its cycle"));
            return;
        }
        int32 DisplayedCharacter=Player->CharacterIndex;
        if(PreviewPage==TEXT("Character")) FParse::Value(FCommandLine::Get(),TEXT("LobbyPreviewCharacter="),DisplayedCharacter);
        const bool bNativeBraxtonReview = !FParse::Param(FCommandLine::Get(), TEXT("BraxtonLegacyVisual"))
            && (PreviewPage == TEXT("Character") || PreviewPage == TEXT("Lobby")) && DisplayedCharacter == 0;
        const bool bNativeFixerReview = !FParse::Param(FCommandLine::Get(), TEXT("FixerLegacyVisual"))
            && (PreviewPage == TEXT("Character") || PreviewPage == TEXT("Lobby")) && DisplayedCharacter == 2;
        const bool bNativeReview = bNativeBraxtonReview || bNativeFixerReview;
        if (Step == 0 && Lobby && Now - Started > (bNativeReview ? 24 : 5) && RenderAssetsReady())
        {
            if (bNativeBraxtonReview)
            {
                int32 NativeActors = 0;
                for (ASeniorBraxtonVisual* Visual : TActorRange<ASeniorBraxtonVisual>(World))
                {
                    AActor* Native = Visual->GetNativeCharacter();
                    if (!Check(Native != nullptr, TEXT("Native Braxton assembly failed to spawn"))) return;
                    ++NativeActors;
                    int32 SimParticles = 0;
                    TInlineComponentArray<USkeletalMeshComponent*> Meshes(Native);
                    for (USkeletalMeshComponent* Mesh : Meshes)
                        if (UClothingSimulationInteractor* Sim = Mesh->GetClothingSimulationInteractor())
                        {
                            SimParticles += Sim->GetNumDynamicParticles();
                            UE_LOG(LogTemp, Display, TEXT("BRAXTON_NATIVE_CLOTH: %s cloths=%d dynamic=%d pinned=%d milliseconds=%.3f"),
                                *Mesh->GetName(), Sim->GetNumCloths(), Sim->GetNumDynamicParticles(), Sim->GetNumKinematicParticles(), Sim->GetSimulationTime());
                            for (const auto& Pair : Mesh->GetCurrentClothingData_GameThread())
                            {
                                FBox Range(ForceInit);
                                for (const FVector3f& P : Pair.Value.Positions) Range += FVector(P);
                                if (!Check(Range.IsValid && Range.GetSize().GetMax() > 20.f
                                    && Range.GetSize().GetMax() < 100.f && !Range.GetCenter().ContainsNaN(),
                                    TEXT("Native cloth collapsed or stretched outside anatomical bounds"))) return;
                                UE_LOG(LogTemp,Display,TEXT("BRAXTON_SIM_RANGE: id=%d lod=%d %s transform=%s"),Pair.Key,Pair.Value.LODIndex,*Range.ToString(),*Pair.Value.Transform.ToString());
                            }
                        }
                    TInlineComponentArray<UGroomComponent*> Grooms(Native);
                    for (UGroomComponent* Groom : Grooms)
                        if (Groom->GetFName() == TEXT("Hair"))
                            UE_LOG(LogTemp, Display, TEXT("BRAXTON_NATIVE_HAIR: enabled=%d lod=%d wind=%s"),
                                Groom->SimulationSettings.SolverSettings.bEnableSimulation, Groom->GetForcedLOD(), *Groom->SimulationSettings.ExternalForces.AirVelocity.ToString());
                    if (!FParse::Param(FCommandLine::Get(), TEXT("LobbyDiagnosticNoCloth"))
                        && !Check(SimParticles > 0, TEXT("Native garment simulation has no moving particles"))) return;
                }
                if (!Check(NativeActors > 0, TEXT("Native Braxton preview was not created"))) return;
            }
            if (bNativeFixerReview)
            {
                int32 VisibleNativeActors = 0;
                for (ASeniorFixerVisual* Visual : TActorRange<ASeniorFixerVisual>(World))
                {
                    AActor* Native = Visual->GetNativeCharacter();
                    if (!Check(Native != nullptr, TEXT("Native Fixer assembly failed to spawn"))) return;
                    if (Native->IsHidden()) continue; // Cached previews need not be active.
                    ++VisibleNativeActors;
                    USkeletalMeshComponent* Body = Visual->GetBodyMesh();
                    if (!Check(Body && Body->GetSkeletalMeshAsset() && Body->IsVisible(),
                        TEXT("Fixer's native body is missing or invisible"))) return;
                    if (!Check(Visual->GetIdleAnimation() && Visual->GetWalkAnimation(),
                        TEXT("Fixer's native idle or walk animation is missing"))) return;
                    if (!Check(FMath::IsNearlyEqual(ASeniorFixerVisual::HeightCm, 168.f, .01f)
                        && Native->GetActorRelativeScale3D().Equals(FVector::OneVector, .001f),
                        TEXT("Fixer must retain the authored 168 cm proportions without extra native-body scaling"))) return;
                    USkeletalMeshComponent* Face = nullptr;
                    USkeletalMeshComponent* Earbuds = nullptr;
                    TInlineComponentArray<USkeletalMeshComponent*> FixerMeshes(Native);
                    for (USkeletalMeshComponent* Mesh : FixerMeshes)
                    {
                        if (Mesh->GetFName() == TEXT("Face")) Face = Mesh;
                        if (Mesh->GetFName() == TEXT("FixerWiredEarbuds")) Earbuds = Mesh;
                    }
                    if (!Check(Face && Face->GetSkeletalMeshAsset() &&
                        Face->GetSkeletalMeshAsset()->GetName().Contains(TEXT("Fixer_Lean")),
                        TEXT("Fixer's independently sculpted face was not loaded"))) return;
                    if (!Check(Earbuds && Earbuds->IsVisible() && Earbuds->GetSkeletalMeshAsset() &&
                        Earbuds->LeaderPoseComponent.Get() == Body &&
                        !Earbuds->Bounds.Origin.ContainsNaN() && Earbuds->Bounds.BoxExtent.GetMax() > 10.f,
                        TEXT("Fixer's wired earbuds are missing or not following his pose"))) return;
                    UE_LOG(LogTemp,Display,TEXT("FIXER_FACE_AND_EARBUDS_VERIFIED face=%s earbuds=%s"),
                        *Face->GetSkeletalMeshAsset()->GetName(),*Earbuds->GetSkeletalMeshAsset()->GetName());
                    UE_LOG(LogTemp, Display, TEXT("FIXER_VISUAL_VERIFIED: body=%s idle=%s walk=%s heightCm=%.1f"),
                        *Body->GetSkeletalMeshAsset()->GetName(), *Visual->GetIdleAnimation()->GetName(),
                        *Visual->GetWalkAnimation()->GetName(), ASeniorFixerVisual::HeightCm);
                }
                if (!Check(VisibleNativeActors > 0, TEXT("Native Fixer preview was not visible"))) return;
            }
            if (PreviewPage != TEXT("Lobby"))
            {
                const auto Atmosphere = GetSeniorLobbyAtmosphereDiagnostics();
                if (!Check(!Atmosphere.bAnimating && Atmosphere.Seconds < .01,
                    TEXT("Lobby animation should pause behind detail pages"))) return;
            }
            FString CharacterList;
            const TCHAR* Variant = FParse::Param(FCommandLine::Get(), TEXT("LobbyPreviewStartMenu")) ? TEXT("_StartMenu") :
                FParse::Param(FCommandLine::Get(), TEXT("LobbyPreviewFace")) ? TEXT("_Face") :
                FParse::Value(FCommandLine::Get(), TEXT("LobbyPreviewCharacters="), CharacterList, false) ? TEXT("_Heights") : TEXT("");
            if (PreviewPage == TEXT("Lobby") && FParse::Value(FCommandLine::Get(), TEXT("LobbyPreviewCharacters="), CharacterList, false))
            {
                int32 PreviewCount = 0;
                float MinHeight = MAX_flt, MaxHeight = 0;
                for (AActor* Actor : TActorRange<AActor>(World))
                {
                    if (!Actor->ActorHasTag(TEXT("SeniorCharacterPreview"))) continue;
                    const auto* Capture = Actor->FindComponentByClass<USceneCaptureComponent2D>();
                    const auto* Mesh = Actor->FindComponentByClass<USkeletalMeshComponent>();
                    if (!Capture || !Mesh || !Mesh->GetSkeletalMeshAsset()) continue;
                    const FBoxSphereBounds Bounds = Mesh->GetSkeletalMeshAsset()->GetBounds();
                    const float Height = float(Bounds.BoxExtent.Z * 2.0);
                    if (!Check(FMath::IsNearlyEqual(Capture->OrthoWidth, 108.f, .01f)
                        && FMath::IsNearlyEqual(float(Capture->GetRelativeLocation().Z), 98.f, .01f)
                        && FMath::IsNearlyZero(float(Mesh->GetRelativeLocation().Z + Bounds.Origin.Z - Bounds.BoxExtent.Z), .01f)
                        && Mesh->GetRelativeScale3D().Equals(FVector::OneVector, .001f),
                        TEXT("Lobby normalized a character height or changed its common floor/camera ruler"))) return;
                    MinHeight = FMath::Min(MinHeight, Height); MaxHeight = FMath::Max(MaxHeight, Height);
                    ++PreviewCount;
                    UE_LOG(LogTemp, Display, TEXT("LOBBY_HEIGHT_PREVIEW: %s height=%.2f commonWidth=%.2f commonAimZ=%.2f"),
                        *Mesh->GetSkeletalMeshAsset()->GetName(), Height, Capture->OrthoWidth, Capture->GetRelativeLocation().Z);
                }
                if (!Check(PreviewCount == 3 && MaxHeight - MinHeight >= 12.f,
                    TEXT("Height screenshot must show three real 3D bodies with visible stature differences"))) return;
            }
            if (PreviewPage == TEXT("Character") && FParse::Param(FCommandLine::Get(), TEXT("LobbyPreviewFace")))
            {
                bool bFaceFramed = false;
                for (AActor* Actor : TActorRange<AActor>(World))
                {
                    if (!Actor->ActorHasTag(TEXT("SeniorCharacterPreview"))) continue;
                    const auto* Capture = Actor->FindComponentByClass<USceneCaptureComponent2D>();
                    const bool bFramedInOriginalPreview = Capture &&
                        Capture->ProjectionType == ECameraProjectionMode::Orthographic &&
                        Capture->OrthoWidth < 30.f && Capture->GetRelativeLocation().Z > 150.f;
                    const bool bFramedInSelectionRoom = Capture &&
                        Capture->ProjectionType == ECameraProjectionMode::Perspective &&
                        Capture->GetRelativeLocation().X < 250.f &&
                        Capture->GetRelativeLocation().Z > (DisplayedCharacter == 2 ? 180.f : 220.f);
                    if (bFramedInOriginalPreview || bFramedInSelectionRoom)
                    {
                        bFaceFramed = true;
                        UE_LOG(LogTemp, Display, TEXT("LOBBY_FACE_PREVIEW: width=%.2f aimZ=%.2f"),
                            Capture->OrthoWidth, Capture->GetRelativeLocation().Z);
                    }
                }
                if (!Check(bFaceFramed, TEXT("Close-up zoom did not move the camera to the face"))) return;
                if (DisplayedCharacter == 0)
                {
                    const auto* Skin = LoadObject<UTexture2D>(nullptr,
                        TEXT("/Game/Characters/Cobble/C01/T_C01_SkinReference.T_C01_SkinReference"));
                    if (!Check(Skin != nullptr, TEXT("Face preview skin texture is missing"))) return;
                    // Await the visible preview's short residency lease, not a global streaming override.
                    if (!bNativeReview && Skin->GetNumResidentMips() < Skin->GetNumMips()) return;
                    UE_LOG(LogTemp, Display, TEXT("LOBBY_FACE_TEXTURE: residentMips=%d totalMips=%d"),
                        Skin->GetNumResidentMips(), Skin->GetNumMips());
                }
            }
            const FString Screenshot = FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/LobbyV2_%s%s.png"), *PreviewPage, Variant);
            UE_LOG(LogTemp, Display, TEXT("LOBBY_VISUAL: Capturing rendered %s page with Slate UI"), *PreviewPage);
            FScreenshotRequest::RequestScreenshot(Screenshot, true, false);
            Step = 1; Changed = Now;
        }
        else if (Step == 1 && Now - Changed > 3)
        { UE_LOG(LogTemp, Display, TEXT("LOBBY_TEST_PASSED: Visual screenshot requested")); FPlatformMisc::RequestExitWithStatus(false, 0); Step = 5; }
        return;
    }
    int32 ExpectedPlayers = (Mode == TEXT("Solo") || Mode == TEXT("Resume")) ? 1 : 3;
    FParse::Value(FCommandLine::Get(), TEXT("LobbyExpectedPlayers="), ExpectedPlayers);
    int32 Character = Mode == TEXT("Client") ? 1 : Mode == TEXT("Solo") ? 2 : 0;
    FParse::Value(FCommandLine::Get(), TEXT("LobbyCharacter="), Character);
    const int32 Loadout = Character % 2;
    const int32 ExpectedDifficulty = Mode == TEXT("Resume") ? 0 : Mode == TEXT("Solo") ? 2 : 1;
    const FString TargetMap = Mode == TEXT("Resume") ? TEXT("Chapter02") : TEXT("Chapter01_House");

    if (Step == 0 && Lobby)
    {
        if (!Check(ASeniorLobbyController::IsValidLANAddress(TEXT("127.0.0.1:17777"))
            && ASeniorLobbyController::IsValidLANAddress(TEXT("192.168.1.25"))
            && !ASeniorLobbyController::IsValidLANAddress(TEXT("127.0.0.1?game=Bad"))
            && !ASeniorLobbyController::IsValidLANAddress(TEXT("1.2.3.999"))
            && !ASeniorLobbyController::IsValidLANAddress(TEXT("1.2.3.4:7.7"))
            && !ASeniorLobbyController::IsValidLANAddress(TEXT("1.2.3.4:65536")), TEXT("IPv4 validation failed"))) return;
        if (Mode == TEXT("Client") && PC->GetNetMode() != NM_Client) return;
        if (Mode != TEXT("Client"))
        {
            if (!Check(PC->IsHost() && State->CanStart(), TEXT("Host could not open the start flow"))) return;
            if(Mode==TEXT("Resume"))
            {
                Story->Progress->Chapter=2; Story->Progress->Checkpoint=1;
                Story->Progress->bHasCheckpoint=true; Story->Progress->bCompleted=false;
                Story->Progress->Spawn=FTransform::Identity; State->bResumeStory=true;
                if(!Check(UGameplayStatics::SaveGameToSlot(Story->Progress,TEXT("SeniorSendoff_LobbyAutomationOnly"),0),TEXT("Could not seed isolated resume checkpoint")))return;
            }
            PC->SetCharacter(1); PC->SetReady(true); PC->SetCharacter(2);
            if (!Check(!Player->bReady, TEXT("Character change must clear ready"))) return;
            PC->SetCharacter(7);
            if (!Check(Player->CharacterIndex == 7 && !Player->bReady, TEXT("Eighth character was not accepted"))) return;
            PC->SetReady(true); PC->SetCharacter(8);
            if (!Check(Player->CharacterIndex == 7 && Player->bReady, TEXT("Invalid ninth character changed selection or readiness"))) return;
            PC->SetCharacter(99);
            if (!Check(Player->CharacterIndex == 7 && Player->bReady, TEXT("Invalid character was accepted"))) return;
            PC->SetCharacter(2);
            PC->SetReady(true); PC->SetLoadout(1);
            if (!Check(!Player->bReady, TEXT("Loadout change must clear ready"))) return;
            Story->SetDifficulty(ExpectedDifficulty);
        }
        PC->SetCharacter(Character); PC->SetLoadout(Loadout);
        if (Mode == TEXT("Client")) PC->StartStory(false); // Server must ignore a non-host start request.
        UE_LOG(LogTemp, Display, TEXT("LOBBY_TEST: %s selected character %d without a ready gate"), *Mode, Character);
        Step = 1; Changed = Now;
    }
    else if (Step == 1)
    {
        if (Mode == TEXT("Client"))
        {
            if (Map != TEXT("Chapter01_House") || !PC->GetPawn()) return;
            if (!Check(Player->CharacterIndex == Character && Player->LoadoutIndex == Loadout, TEXT("Client selections lost in chapter travel"))) return;
            auto* CharacterPawn = Cast<AStoryFirstPersonCharacter>(PC->GetPawn());
            if (CharacterPawn && CharacterPawn->GetPlayerState() != Player) return;
            if (!CheckCharacterAppearance(CharacterPawn)) return;
            UE_LOG(LogTemp, Display, TEXT("LOBBY_TEST: Client reached chapter with selections and pawn"));
            Step = 3; Changed = Now;
            return;
        }
        if (!Lobby || State->GetMembers().Num() != ExpectedPlayers || !State->CanStart() || Now - Changed < 1) return;
        if (!Check(Player->CharacterIndex == Character && Player->LoadoutIndex == Loadout, TEXT("Host selections wrong"))) return;
        PC->StartStory(Mode==TEXT("Resume"));
        if (!Check(Story->bTravelPending && State->bStarting, TEXT("Host did not start story"))) return;
        UE_LOG(LogTemp, Display, TEXT("LOBBY_TEST: Host started with %d members"), ExpectedPlayers);
        Step = 2; Changed = Now;
    }
    else if (Step == 2 && Map == TargetMap && PC->GetPawn() && Now - Changed > 3)
    {
        if(bDouliNetworkTest && Now-DouliChapterStart<14)return;
        if (!Check(Player->CharacterIndex == Character && Player->LoadoutIndex == Loadout, TEXT("Host/solo selections lost in travel"))) return;
        if (!Check(Story->Progress && Story->Progress->Difficulty == ExpectedDifficulty, TEXT("Selected difficulty was not saved into the story"))) return;
        if (State->GetMembers().Num() != ExpectedPlayers) return;
        int32 Pawns = 0;
        for (TActorIterator<AStoryFirstPersonCharacter> It(World); It; ++It)
            if (It->GetController())
            {
                if (!CheckCharacterAppearance(*It)) return;
                ++Pawns;
            }
        if (Pawns != ExpectedPlayers) return;
        UE_LOG(LogTemp, Display, TEXT("LOBBY_TEST: Chapter loaded with %d possessed first-person pawns"), Pawns);
        PC->ReturnToLobby(); Step = 3; Changed = Now;
    }
    else if (Step == 3 && Lobby && State->GetMembers().Num() == ExpectedPlayers)
    {
        if (Now - Changed < 1) return;
        if (!Check(Player->CharacterIndex == Character && Player->LoadoutIndex == Loadout, TEXT("Selections lost on return to lobby"))) return;
        if (!Check(!Player->bReady && !State->bStarting && State->CanStart(), TEXT("Return to lobby did not restore the host start flow"))) return;
        UE_LOG(LogTemp, Display, TEXT("LOBBY_TEST: Returned to lobby with %d members"), ExpectedPlayers);
        Step = 4; Changed = Now;
    }
    else if (Step == 4 && Now - Changed > (Mode == TEXT("Client") ? 2 : 3))
    {
        UE_LOG(LogTemp, Display, TEXT("LOBBY_TEST_PASSED: %s start flow, difficulty, selections, checkpoint travel and lobby return"), *Mode);
        FPlatformMisc::RequestExitWithStatus(false, 0);
        Step = 5;
    }
}
#endif
