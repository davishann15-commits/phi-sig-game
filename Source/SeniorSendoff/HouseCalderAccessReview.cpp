// Opt-in physical proof for the added South Lot tier connection. The original
// FreeRoam routes, player settings and all existing review limits stay fixed.
#if !UE_BUILD_SHIPPING
#include "HouseCalderFeatures.h"
#include "HouseCalderLandform.h"
#include "HouseCalderTerrain.h"
#include "HouseExteriorSiteLayout.h"
#include "StoryCampaign.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace HouseCalderAccessReviewImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Walker;
    double Start = 0.0, LegStart = 0.0, Progress = 0.0, FinishAt = 0.0;
    int32 Phase = 0, Goal = 0, Reached = 0;
    float Best = MAX_flt;
    FVector Target;
    FString Dir, Report;
    TArray<FString> Captures;
};
FState& State() { static FState S; return S; }
const FVector2D Goals[] = { {1800,-22500}, {1800,-24200}, {1800,-22500}, {1800,-20850} };

bool Floor(UWorld* W, AActor* Ignore, const FVector2D& P, float& Z, bool bEntranceProfile = false, FHitResult* OutHit = nullptr)
{
    float Datum = HouseExteriorSite::GroundHeight(P.X, P.Y);
    if (bEntranceProfile && !HouseCalderFeatures::FloorAt(P, Datum)) Datum = HouseCalderTerrain::SurfaceZ(P.X, P.Y);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CalderAccessFloor), true);
    Query.AddIgnoredActor(Ignore);
    FHitResult Hit;
    // Extended entrance profiles pass below canopies. Use the expected walking
    // surface only to place the ray; its floor height and normal still come
    // from world collision. The original lot profiles retain their old ray.
    const float Above = bEntranceProfile ? 40.f : 500.f;
    if (!W->LineTraceSingleByChannel(Hit, FVector(P, Datum + Above), FVector(P, Datum - 500.f), ECC_Visibility, Query)) return false;
    if (OutHit) *OutHit = Hit;
    Z = Hit.ImpactPoint.Z;
    return Hit.ImpactNormal.Z >= .766f;
}
void Note(FState& S, const FString& Line)
{
    S.Report += Line + TEXT("\n");
    UE_LOG(LogTemp, Display, TEXT("CALDER_ACCESS %s"), *Line);
}
void Finish(FState& S, bool bPassed, const FString& Reason)
{
    if (S.Walker.IsValid()) S.Walker->GetCharacterMovement()->StopMovementImmediately();
    S.Phase = -1;
    Note(S, FString::Printf(TEXT("%s reached=%d/4 terrainHash=%08x %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"),
        S.Reached, HouseCalderTerrain::Hash(), *Reason));
    FFileHelper::SaveStringToFile(S.Report, *(S.Dir / TEXT("result.txt")));
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

// Independent world-collision profiles across the centre and both walking
// sides. Include both outer curbs, not only the generated tread range.
bool Profile(UWorld* W, AActor* Ignore, FState& S)
{
    int32 Flights = 0;
    bool bAll = true;
    for (const HouseCalderLandform::FStair& D : HouseCalderLandform::Stairs())
    {
        if (!D.Name.StartsWith(TEXT("walk south_lot_tier_links ")) && D.Name != TEXT("walk walk_45 stair")
            && D.Name != TEXT("walk walk_00 stair") && D.Name != TEXT("walk walk_29 stair")
            && D.Name != TEXT("walk walk_47 flight")) continue;
        ++Flights;
        const FVector2D Direction = (D.Head - D.Foot).GetSafeNormal();
        const FVector2D Side(-Direction.Y, Direction.X);
        const double Length = (D.Head - D.Foot).Size();
        const bool bEntranceProfile = D.Name == TEXT("walk walk_00 stair") || D.Name == TEXT("walk walk_29 stair")
            || D.Name == TEXT("walk walk_47 flight");
        const int32 ExpectedJumps = D.Name == TEXT("walk walk_00 stair") ? 22
            : D.Name == TEXT("walk walk_29 stair") ? 9
            : D.Name == TEXT("walk walk_47 flight") ? 66
            : D.Name == TEXT("walk walk_45 stair") ? 7 : (FMath::Abs(D.HeadRelief - D.FootRelief) < 350.f ? 20 : 26);
        for (const double Across : {-(D.Width * .5 - 25.0), 0.0, D.Width * .5 - 25.0})
        {
            int32 Jumps = 0, Missing = 0, DetailCount = 0;
            float Previous = 0.f, MinRise = MAX_flt, MaxRise = 0.f;
            double LastJump = -1.e9, MinGoing = 1.e9;
            bool bHavePrevious = false, bOK = true;
            const int32 Samples = FMath::CeilToInt(Length + 440.0);
            const bool bMusic = D.Name == TEXT("walk walk_29 stair");
            const double ProfileStart = bMusic ? -14.0 : -220.0;
            const double ProfileLength = bMusic ? Length + 16.0 : double(Samples);
            for (int32 K = 0; K <= Samples; ++K)
            {
                // Keep the same number of checks at Music, with denser
                // spacing over its actual exterior threshold/stair/join.
                // The old 220 cm prefix entered an unloaded interior.
                const double Along = ProfileStart + ProfileLength * K / Samples;
                const FVector2D P = D.Foot + Direction * Along + Side * Across;
                float Z = 0.f;
                FHitResult FloorHit;
                if (!Floor(W, Ignore, P, Z, bEntranceProfile, &FloorHit))
                {
                    ++Missing; bHavePrevious = false;
                    if (DetailCount++ < 3) Note(S, FString::Printf(TEXT("PROFILE_POINT missing name=\"%s\" along=%.3f across=%.1f xy=(%.3f,%.3f) hit=%s normalZ=%.3f"),
                        *D.Name, Along, Across, P.X, P.Y, *GetNameSafe(FloorHit.GetComponent()), FloorHit.ImpactNormal.Z));
                    continue;
                }
                if (bHavePrevious)
                {
                    const float Rise = FMath::Abs(Z - Previous);
                    if (Rise > 5.f)
                    {
                        ++Jumps; MinRise = FMath::Min(MinRise, Rise); MaxRise = FMath::Max(MaxRise, Rise);
                        if (LastJump > -1.e8) MinGoing = FMath::Min(MinGoing, Along - LastJump);
                        LastJump = Along;
                        bOK &= Rise >= 13.9f && Rise <= 18.1f;
                        if ((Rise < 13.9f || Rise > 18.1f) && DetailCount++ < 3)
                            Note(S, FString::Printf(TEXT("PROFILE_POINT badRise name=\"%s\" along=%.3f across=%.1f xy=(%.3f,%.3f) before=%.3f after=%.3f hit=%s"),
                                *D.Name, Along, Across, P.X, P.Y, Previous, Z, *GetNameSafe(FloorHit.GetComponent())));
                    }
                }
                Previous = Z; bHavePrevious = true;
            }
            bOK &= Missing == 0 && Jumps == ExpectedJumps && MinGoing >= 29.9;
            bAll &= bOK;
            Note(S, FString::Printf(TEXT("PROFILE %s flight=%d across=%.0f samples=%d jumps=%d/%d missing=%d riser=%.2f..%.2f goingMin=%.1f"),
                bOK ? TEXT("PASS") : TEXT("FAIL"), Flights, Across, Samples + 1, Jumps, ExpectedJumps, Missing, MinRise, MaxRise, MinGoing));
        }
    }
    return bAll && Flights == 6;
}
void Tick(UWorld* W, ELevelTick, float)
{
    static const bool bRun = FParse::Param(FCommandLine::Get(), TEXT("CalderAccessReview"));
    if (!bRun || !W || !W->IsGameWorld() || UGameplayStatics::GetCurrentLevelName(W, true) != TEXT("Chapter01_House")) return;
    FState& S = State();
    const double Now = W->GetTimeSeconds();
    if (S.World.Get() != W) { S = FState(); S.World = W; S.Start = Now; }
    if (S.Phase < 0) return;
    APlayerController* PC = W->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn = PC ? Cast<AStoryFirstPersonCharacter>(PC->GetPawn()) : nullptr;
    if (!Pawn || Now - S.Start < 8.0 || !HouseCalderTerrain::CollisionReady(W)) return;
    S.Walker = Pawn;
    if (S.Phase == 0)
    {
        S.Dir = FPaths::ProjectSavedDir() / TEXT("CalderAccessReview");
        IFileManager::Get().MakeDirectory(*S.Dir, true);
        IFileManager::Get().Delete(*(S.Dir / TEXT("result.txt")));
        if (!HouseCalderFeatures::Active() || !Profile(W, Pawn, S)) { Finish(S, false, TEXT("physical step/curb profile failed")); return; }
        float Z = 0.f;
        if (!Floor(W, Pawn, FVector2D(1800,-20850), Z)) { Finish(S, false, TEXT("no start floor")); return; }
        const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        // One setup teleport; every subsequent leg uses ordinary input.
        Pawn->SetActorLocation(FVector(1800,-20850,Z+Half+4.f), false, nullptr, ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        S.Phase = 1; S.Goal = 0;
    }
    if (S.Phase == 1)
    {
        float Z = 0.f;
        if (!Floor(W, Pawn, Goals[S.Goal], Z)) { Finish(S, false, TEXT("no goal floor")); return; }
        S.Target = FVector(Goals[S.Goal], Z);
        S.LegStart = S.Progress = Now; S.Best = MAX_flt; S.Phase = 2;
        PC->SetControlRotation(FRotator(-5.f, (S.Target - Pawn->GetActorLocation()).Rotation().Yaw, 0.f));
        return;
    }
    if (S.Phase == 2)
    {
        const FVector Feet = Pawn->GetActorLocation() - FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        const float Distance = FVector::Dist2D(Feet, S.Target);
        if (Distance < S.Best - 5.f) { S.Best = Distance; S.Progress = Now; }
        if (Now - S.Progress > 8.0 || Now - S.LegStart > 30.0)
        { Finish(S, false, FString::Printf(TEXT("walking stalled at %s goal=%d distance=%.1f"), *Feet.ToString(), S.Goal, Distance)); return; }
        if (Distance > 35.f || FMath::Abs(Feet.Z - S.Target.Z) > 8.f) return;
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        ++S.Reached;
        Note(S, FString::Printf(TEXT("WALK reached=%d/4 feet=%s realMovementInput=1"), S.Reached, *Feet.ToString()));
        const FString Capture = S.Dir / FString::Printf(TEXT("SouthLot_%d.png"), S.Reached);
        IFileManager::Get().Delete(*Capture);
        FScreenshotRequest::RequestScreenshot(Capture, false, false);
        S.Captures.Add(Capture);
        if (++S.Goal < int32(UE_ARRAY_COUNT(Goals))) S.Phase = 1;
        else { S.Phase = 3; S.FinishAt = Now + 2.0; }
        return;
    }
    if (S.Phase == 3 && Now >= S.FinishAt && !FScreenshotRequest::IsScreenshotRequested())
    {
        bool bImages = S.Captures.Num() == 4;
        for (const FString& Capture : S.Captures) bImages &= IFileManager::Get().FileSize(*Capture) > 0;
        Finish(S, bImages, TEXT("two flights and their approach curbs; uphill/downhill; four saved captures"));
    }
}
struct FRegistration
{
    FRegistration()
    {
        FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);
        FCoreDelegates::OnBeginFrame.AddLambda([]
        {
            FState& S = State();
            if (S.Phase == 2 && S.Walker.IsValid())
                S.Walker->AddMovementInput((S.Target - S.Walker->GetActorLocation()).GetSafeNormal2D(), 1.f);
        });
    }
};
FRegistration Registration;
}
#endif
