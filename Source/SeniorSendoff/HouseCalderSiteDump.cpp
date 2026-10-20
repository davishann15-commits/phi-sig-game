// -CalderSiteDump (Calder terrain job, step 1a; run it alone, it ends the run): what the map itself places, for the
// terrain plan. Logs every PlayerStart and the fall-respawn limit computed exactly as AStoryGameMode does
// (StoryCampaign.cpp:1761-1763: the first PlayerStart's Z - 1200), then writes every actor loaded with the map (net startup
// actors: PlayerStarts, checkpoints, the pigs, triggers, volumes, the house's placed props) with its class, name, location
// and bounds to Saved/CalderSiteDump/actors.json, so the plan can hold the relief at zero round each one.
#if !UE_BUILD_SHIPPING
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace HouseCalderSiteDumpImpl
{
struct FState { TWeakObjectPtr<UWorld> World; double Start = 0.0; bool bDone = false; };
FState& State() { static FState S; return S; }

FString Vec(const FVector& V) { return FString::Printf(TEXT("[%.1f,%.1f,%.1f]"), V.X, V.Y, V.Z); }

void Tick(UWorld* W, ELevelTick, float)
{
    if (!W || !W->IsGameWorld() || W->GetNetMode() == NM_Client || !FParse::Param(FCommandLine::Get(), TEXT("CalderSiteDump"))) return;
    if (UGameplayStatics::GetCurrentLevelName(W, true) != TEXT("Chapter01_House")) return;
    FState& S = State();
    const double Now = FPlatformTime::Seconds();
    if (S.World.Get() != W) { S = FState(); S.World = W; S.Start = Now; }
    if (S.bDone || Now - S.Start < 25.0) return;   // after the exterior and campus have built
    S.bDone = true;

    TArray<AActor*> Starts;
    UGameplayStatics::GetAllActorsOfClass(W, APlayerStart::StaticClass(), Starts);
    for (int32 I = 0; I < Starts.Num(); ++I)
        UE_LOG(LogTemp, Display, TEXT("CALDER_SITE playerStart[%d] %s at %s"), I, *Starts[I]->GetName(), *Starts[I]->GetActorLocation().ToCompactString());
    const float FallLimit = Starts.Num() ? float(Starts[0]->GetActorLocation().Z - 1200.0) : -1500.f;
    UE_LOG(LogTemp, Display, TEXT("CALDER_SITE fallLimit=%.1f (first PlayerStart Z - 1200, as AStoryGameMode) walkableFloor=%.1f"), FallLimit, FallLimit + 400.f);

    FString Json = FString::Printf(TEXT("{\n\"fallLimit\": %.1f,\n\"playerStarts\": ["), FallLimit);
    for (int32 I = 0; I < Starts.Num(); ++I) Json += FString::Printf(TEXT("%s%s"), I ? TEXT(",") : TEXT(""), *Vec(Starts[I]->GetActorLocation()));
    Json += TEXT("],\n\"actors\": [\n");
    int32 Count = 0;
    TMap<FString, int32> ByClass;
    for (TActorIterator<AActor> It(W); It; ++It)
    {
        AActor* A = *It;
        if (!A->IsNetStartupActor()) continue;
        FVector Origin, Extent;
        A->GetActorBounds(false, Origin, Extent);
        const FString Class = A->GetClass()->GetName();
        ByClass.FindOrAdd(Class)++;
        Json += FString::Printf(TEXT("%s{\"class\":\"%s\",\"name\":\"%s\",\"at\":%s,\"origin\":%s,\"extent\":%s}"), Count ? TEXT(",\n") : TEXT(""),
            *Class, *A->GetName(), *Vec(A->GetActorLocation()), *Vec(Origin), *Vec(Extent));
        ++Count;
    }
    Json += TEXT("\n]\n}\n");
    const FString Path = FPaths::ProjectSavedDir() / TEXT("CalderSiteDump/actors.json");
    FFileHelper::SaveStringToFile(Json, *Path);
    ByClass.ValueSort([](int32 A, int32 B) { return A > B; });
    FString Classes;
    for (const TPair<FString, int32>& P : ByClass) Classes += FString::Printf(TEXT(" %s=%d"), *P.Key, P.Value);
    UE_LOG(LogTemp, Display, TEXT("CALDER_SITE startupActors=%d ->%s;%s"), Count, *Path, *Classes);
    FPlatformMisc::RequestExitWithStatus(false, 0);
}

struct FRegistration { FRegistration() { FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick); } };
FRegistration Registration;
}
#endif
