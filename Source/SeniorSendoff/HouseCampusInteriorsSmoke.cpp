#include "HouseFraternityRow.h"
// -CampusInteriorsSmoke=LIB,HAR,SCI,UNI,STA (or =TierA; =All adds Tier B and C): a smoke test for each walk-in building,
// in the style of the house's room tests (HouseLibrarySmoke and the others): opt-in, no normal-game changes. For each
// building it builds the inside where it stands and checks what it is made of:
// - geometry: enough triangles, house kit pieces and furniture for its tier; every solid part's collision is cooked and
//   in the physics scene; every part drawn by its static copy;
// - floors: under every pooled light's anchor a trace finds the building's own walkable surface (a floor, a stair, a
//   tier, a landing), not the ground outside;
// - props: the house furniture it spawned (ACampusProp) all rest on something, within 3 cm (none floating);
// - doors: every door the building specifies, by name, standing where it should and locked exactly when it has a reason
//   for the HUD; the main ones unlocked;
// - wall clocks: at least one, every hand on its dial and within a degree of the clock tower's time (the tower must be
//   there); the dial's printing is checked by eye, in the walk's shots;
// - signs: its own sign is on one of its built parts; its sounds playing (when sound is on; the JSON says when skipped);
// - windows: the plan opens windows on its open floors (plan buildings; the cut glass itself is seen in the shots).
// Run it alone (it builds each inside where it stands). Writes Saved/CampusInteriorsSmoke/<Id>.json and result.txt,
// logs CAMPUS_SMOKE_<Id>_PASSED or _FAILED, then exits; it fails (rather than waits) when the interiors are off or it
// runs out of time.
#include "HouseCampusInteriors.h"
#include "HouseCampusInteriorsRooms.h"
#include "HouseBlackHallLook.h"
#include "HouseCalderTerrain.h"
#include "HouseCampusLayout.h"
#include "HouseClockTower.h"
#include "HouseExterior.h"
#if !UE_BUILD_SHIPPING
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace HouseCampusInteriorsSmokeImpl
{
struct FState
{
    TArray<FString> Ids;
    int32 Index = 0, Phase = 0, Passed = 0;
    double Started = 0.0, Step = 0.0;
    FString Dir, Summary;
};
FState& State() { static FState S; return S; }
/** A review on the command line that always ends the run itself: then a pass here leaves the ending to it. */
bool OthersEndTheRun()
{
    static const TCHAR* const SelfExiting[] = {
        TEXT("FreeRoamWalkReview"), TEXT("ParkingLotWalkReview"), TEXT("WoodsWalkReview"), TEXT("ClockTowerReview"), TEXT("HouseExteriorViews"),
        TEXT("HouseExteriorWalkReview"), TEXT("HouseWalkPerf"), TEXT("CampusDrumsRecord"), TEXT("WestFieldWalkReview"), TEXT("CalderTerrainReview"), TEXT("CalderPigVisualReview"), TEXT("CalderAccessReview"), TEXT("CalderFarmSolidWalkReview"), TEXT("CalderWildlifeReview"), TEXT("CalderWildlifePathsReview"), TEXT("CalderWildlifeNetworkReview"), TEXT("CalderWildlifeBirdArtReview"), TEXT("CalderWildlifeSkyPathsReview"), TEXT("CalderWildlifeSkyReview"), TEXT("CalderGoosePondReview"), TEXT("CalderGoosePondNetworkReview"), TEXT("CalderReachReview"), TEXT("CalderLandscapeReview"), TEXT("CalderTreePreview"), TEXT("CalderTreeFitReview"), TEXT("CalderRidgeForestReview") };
    for (const TCHAR* Name : SelfExiting) if (FParse::Param(FCommandLine::Get(), Name)) return true;
    return false;
}
/** A sign each building paints for itself (its look ran). */
const TCHAR* SignKey(const FString& Id)
{
    if (Id == TEXT("LIB")) return TEXT("CI_SignCirculation");
    if (Id == TEXT("HAR")) return TEXT("CI_SignHarlow");
    if (Id == TEXT("SCI")) return TEXT("CI_SciDirectory");
    if (Id == TEXT("UNI")) return TEXT("CI_UniDirectory");
    if (Id == TEXT("STA")) return TEXT("CI_StaGate4");
    if (Id == TEXT("TWR")) return TEXT("CI_TwrNotice");
    return nullptr;   // Tier B and C: the generator's own hanging sign, CI_Gen<Id>Sign
}
}

void TickHouseCampusInteriorsSmoke(UWorld* W)
{
    namespace SI = HouseCampusInteriorsSmokeImpl;
    SI::FState& S = SI::State();
    if (S.Phase < 0 || !W || !W->IsGameWorld()) return;
    if (S.Phase == 0 && S.Ids.Num() == 0)
    {
        FString List;
        if (!FParse::Value(FCommandLine::Get(), TEXT("CampusInteriorsSmoke="), List, false)) { S.Phase = -1; return; }
        if (List.Equals(TEXT("TierA"), ESearchCase::IgnoreCase) || List.IsEmpty()) List = TEXT("LIB,HAR,SCI,UNI,STA,TWR");
        if (List.Equals(TEXT("All"), ESearchCase::IgnoreCase)) List = TEXT("LIB,HAR,SCI,UNI,STA,TWR,MAIN,HLTH,COM,ASH,HUM,KER,FLD,ART,MUS,ENG,PLT,ALU");
        if(List.Equals(TEXT("LIB,HAR,SCI,UNI,STA,TWR,MAIN,HLTH,COM,ASH,HUM,KER,FLD,ART,MUS,ENG,PLT,ALU"))&&HouseFraternityRow::Enabled())List+=TEXT(",R1,R2,R3,R4,R5");
        List.ParseIntoArray(S.Ids, TEXT(","));
        if (S.Ids.Num() == 0) S.Ids = { TEXT("LIB"), TEXT("HAR"), TEXT("SCI"), TEXT("UNI"), TEXT("STA"), TEXT("TWR") };
        S.Started = W->GetTimeSeconds();
        S.Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CampusInteriorsSmoke")));
        IFileManager::Get().DeleteDirectory(*S.Dir, false, true);
        IFileManager::Get().MakeDirectory(*S.Dir, true);
    }
    UCampusInteriorsSubsystem* Sub = W->GetSubsystem<UCampusInteriorsSubsystem>();
    AHouseCampusInteriors* Interiors = Sub ? Sub->GetInteriors() : nullptr;
    const double Now = W->GetTimeSeconds();
    // Nothing to test (the interiors off, another map) or stuck: a failure with its reason, never a wait.
    FString Abort;
    if (!HouseCampusInteriors::Enabled()) Abort = TEXT("the interiors are off (-NoCampusInteriors or -NoCampusLayout)");
    else if (Now - S.Started > 30.0 + 15.0 * S.Ids.Num()) Abort = Interiors ? TEXT("timed out") : TEXT("no interiors actor (not Chapter01_House?)");
    if (!Abort.IsEmpty()) { S.Summary += TEXT("FAIL ") + Abort + TEXT("\n"); S.Index = S.Ids.Num(); }
    else if (!Interiors || Now - S.Started < 8.0 || !HouseCalderTerrain::CollisionReady(W)) return;   // (and the terrain's collision cooked)
    if (!S.Ids.IsValidIndex(S.Index))
    {
        const bool bAll = Abort.IsEmpty() && S.Passed == S.Ids.Num();
        S.Summary = FString::Printf(TEXT("%s buildings=%d/%d\n"), bAll ? TEXT("PASS") : TEXT("FAIL"), S.Passed, S.Ids.Num()) + S.Summary;
        FFileHelper::SaveStringToFile(S.Summary, *(S.Dir / TEXT("result.txt")));
        UE_LOG(LogTemp, Display, TEXT("CAMPUS_SMOKE %s passed=%d/%d%s"), bAll ? TEXT("PASS") : TEXT("FAIL"), S.Passed, S.Ids.Num(), *(Abort.IsEmpty() ? FString() : TEXT(" ") + Abort));
        S.Phase = -1;
        if (!bAll || !SI::OthersEndTheRun()) FPlatformMisc::RequestExitWithStatus(false, bAll ? 0 : 1);
        return;
    }
    const FString Id = S.Ids[S.Index].TrimStartAndEnd();
    const int32 Seed = HouseCampusInteriors::SeedFor(*Id);
    const HouseCampusInteriorsImpl::FInteriorDef* Def = HouseCampusInteriorsImpl::InteriorFor(Seed);
    if (S.Phase == 0)
    {
        if (!Def) { S.Summary += FString::Printf(TEXT("%s: FAIL unknown building\n"), *Id); ++S.Index; return; }
        if (!Interiors->Loaded().Contains(Seed)) Interiors->BuildNow(Seed);
        S.Step = Now;
        S.Phase = 1;
        return;
    }
    // Give the collision its asynchronous cook, the clocks their first update and the parts their static copies (one
    // piece every 0.2 s; at most 8 s).
    if (Now - S.Step < 3.0 || (Interiors->PendingConversions(Seed) > 0 && Now - S.Step < 8.0)) return;
    TArray<FString> Problems;
    TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
    const FTransform Frame = HouseCampusInteriors::Frame(Seed);
    const bool bPlan = Seed < HouseCampusInteriors::FreeSeedBase;
    const float Height = bPlan ? HouseCampusLayout::Blocks()[Seed].Height : 0.f;
    // Geometry.
    AHouseCampusInteriors::FStats St;
    if (!Interiors->Stats(Seed, St)) Problems.Add(TEXT("not built"));
    // What a building of its tier should at least hold (a Tier C lobby is one room; Tier A, whole floors).
    const TCHAR Tier = bPlan ? HouseCampusLayout::Blocks()[Seed].Tier : TEXT('A');
    const int32 MinTris = Tier == TEXT('A') ? 15000 : Tier == TEXT('B') ? 5000 : 1000, MinAnchors = Id==TEXT("R1") ? 1 : Tier == TEXT('A') ? 6 : Tier == TEXT('B') ? 4 : 1;
    const int32 MinKit = Tier == TEXT('A') ? 5 : 1, MinProps = Tier == TEXT('A') ? 20 : 1;
    if (St.Triangles < MinTris) Problems.Add(FString::Printf(TEXT("only %d triangles"), St.Triangles));
    if (St.Decor < MinKit) Problems.Add(FString::Printf(TEXT("only %d pieces of the house's kit"), St.Decor));
    // Each part drawn by its static copy by now (one is made a tick, Phase 4), the procedural one hidden.
    if (W->GetNetMode() != NM_DedicatedServer && St.DrawnParts != St.Parts) Problems.Add(FString::Printf(TEXT("%d of %d parts drawn as static meshes"), St.DrawnParts, St.Parts));
    if (St.SolidParts == 0 || St.CollidingParts < St.SolidParts) Problems.Add(FString::Printf(TEXT("solid parts with collision %d/%d"), St.CollidingParts, St.SolidParts));
    Report->SetNumberField(TEXT("parts"), St.Parts); Report->SetNumberField(TEXT("triangles"), St.Triangles); Report->SetNumberField(TEXT("houseKitPieces"), St.Decor);
    // Floors under the light anchors.
    TArray<FVector> Anchors;
    if (Def->Anchors) Def->Anchors(Seed, Height, Anchors);
    // Every light hangs over something walkable (a floor, a stair, a tier, a landing): none over a void.
    int32 FloorHits = 0;
    float MaxDrop = 0.f;
    TArray<TSharedPtr<FJsonValue>> Voids;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CampusSmokeFloor), true);
    for (const FVector& A : Anchors)
    {
        const FVector From = Frame.TransformPosition(A);
        FHitResult Hit;
        // (Down far enough for a light over a stair well or an atrium, or high in the stadium's concourse.) Its own surface:
        // the ground outside, under a missing slab, must not count; the building's furniture under a light is looked past;
        // the stadium's shell, stair tower and press box are the exterior's (built for the walk-in), so there it counts.
        FCollisionQueryParams Under = Q;
        bool bHit = false;
        for (int32 Try = 0; Try < 4; ++Try)
        {
            bHit = W->LineTraceSingleByChannel(Hit, From, From - FVector(0, 0, 900.f), ECC_Pawn, Under);
            const ACampusProp* Prop = bHit ? Cast<ACampusProp>(Hit.GetActor()) : nullptr;
            if (!Prop || Prop->Seed != Seed) break;
            Under.AddIgnoredActor(Prop);
        }
        const AActor* Owner = bHit ? Hit.GetActor() : nullptr;
        const bool bOwn = Owner && (Owner == Interiors || (Id == TEXT("STA") && Owner->IsA<AHouseExteriorDetail>()));
        if (bHit && Hit.ImpactNormal.Z > .7f && bOwn)
        {
            ++FloorHits;
            MaxDrop = FMath::Max(MaxDrop, float(From.Z - Hit.ImpactPoint.Z));
        }
        else Voids.Add(MakeShared<FJsonValueString>(From.ToCompactString() + (Hit.GetActor() ? TEXT(" hit ") + Hit.GetActor()->GetName() : FString())));
    }
    if ((Id==TEXT("R1")&&Anchors.Num()!=1) || Anchors.Num() < MinAnchors || FloorHits < Anchors.Num()) Problems.Add(FString::Printf(TEXT("a walkable floor under %d of %d light anchors"), FloorHits, Anchors.Num()));
    Report->SetArrayField(TEXT("anchorsOverNothing"), Voids);
    Report->SetNumberField(TEXT("lightAnchors"), Anchors.Num()); Report->SetNumberField(TEXT("floorsUnderAnchors"), FloorHits); Report->SetNumberField(TEXT("deepestDropUnderALight"), MaxDrop);
    // Props: every one rests on something.
    int32 Props = 0, Floating = 0;
    TArray<TSharedPtr<FJsonValue>> FloatingList;
    for (TActorIterator<ACampusProp> It(W); It; ++It)
    {
        if (It->Seed != Seed) continue;
        ++Props;
        FVector Origin, Extent;
        It->GetActorBounds(true, Origin, Extent);
        FCollisionQueryParams PQ(SCENE_QUERY_STAT(CampusSmokeProp), true, *It);
        FHitResult Hit;
        if (!W->LineTraceSingleByChannel(Hit, Origin, Origin - FVector(0, 0, Extent.Z + 3.f), ECC_Visibility, PQ))
        {
            ++Floating;
            FloatingList.Add(MakeShared<FJsonValueString>(It->GetName() + TEXT(" kit ") + FString::FromInt(It->KitIndex) + TEXT(" at ") + It->GetActorLocation().ToCompactString()));
        }
    }
    Report->SetArrayField(TEXT("propsFloatingList"), FloatingList);
    if (Floating > 0 || Props < MinProps) Problems.Add(FString::Printf(TEXT("%d of %d props float (at least %d wanted)"), Floating, Props, MinProps));
    Report->SetNumberField(TEXT("props"), Props); Report->SetNumberField(TEXT("propsFloating"), Floating);
    // Doors.
    TArray<HouseCampusInteriorsImpl::FDoorSpec> Specs;
    if (Def->Doors) Def->Doors(Seed, Height, Specs);
    int32 Doors = 0, Locked = 0, Reasons = 0, MainOK = 0, Mains = 0;
    const FString Prefix = FString::Printf(TEXT("Campus_%s_"), *Id);
    TArray<TSharedPtr<FJsonValue>> DoorList;
    TMap<FString, const ACampusDoor*> ByName;
    for (TActorIterator<ACampusDoor> It(W); It; ++It)
    {
        const FString Name = It->DoorId.ToString();
        if (!Name.StartsWith(Prefix)) continue;
        if (ByName.Contains(Name)) Problems.Add(TEXT("two doors named ") + Name);
        ByName.Add(Name, *It);
        ++Doors;
        DoorList.Add(MakeShared<FJsonValueString>(Name.RightChop(Prefix.Len()) + (It->LockedReason.IsEmpty() ? FString(TEXT(": open")) : TEXT(": locked - ") + It->LockedReason)));
        const bool bLocked = !It->LockedReason.IsEmpty();
        if (bLocked) { ++Locked; Reasons += It->LockedReason.Len() > 8 ? 1 : 0; }
        if (Name.EndsWith(TEXT("_MainA")) || Name.EndsWith(TEXT("_MainB"))) { ++Mains; MainOK += bLocked ? 0 : 1; }
    }
    if (Doors != Specs.Num()) Problems.Add(FString::Printf(TEXT("%d doors for %d specified"), Doors, Specs.Num()));
    for (const HouseCampusInteriorsImpl::FDoorSpec& D : Specs)
    {
        const ACampusDoor* const* Found = ByName.Find(Prefix + D.Name.ToString());
        if (!Found) { Problems.Add(TEXT("no door ") + D.Name.ToString()); continue; }
        const FVector At = (*Found)->GetActorLocation();
        if (FMath::Min(FVector::Dist2D(At, Frame.TransformPosition(D.Hinge)), FVector::Dist2D(At, Frame.TransformPosition(D.Center))) > 200.f)
            Problems.Add(TEXT("door ") + D.Name.ToString() + TEXT(" away from its opening"));
        if (D.Reason.IsEmpty() != (*Found)->LockedReason.IsEmpty()) Problems.Add(TEXT("door ") + D.Name.ToString() + TEXT(" locked when it should not be, or open when it should be locked"));
    }
    if (Mains == 0 || MainOK < Mains) Problems.Add(FString::Printf(TEXT("main doors open %d/%d"), MainOK, Mains));
    if (Reasons < Locked) Problems.Add(FString::Printf(TEXT("locked doors with a reason %d/%d"), Reasons, Locked));
    Report->SetArrayField(TEXT("doorList"), DoorList);
    Report->SetNumberField(TEXT("doors"), Doors); Report->SetNumberField(TEXT("lockedDoors"), Locked); Report->SetNumberField(TEXT("lockedWithReason"), Reasons);
    // Wall clocks.
    int32 Clocks = 0;
    const float ClockErr = Interiors->ClockError(Seed, Clocks);
    if (!AHouseClockTower::Find(W)) Problems.Add(TEXT("no clock tower to keep time with"));
    if (Clocks == 0 || ClockErr < 0.f || ClockErr > 1.f || St.Hands != Clocks * 2) Problems.Add(FString::Printf(TEXT("wall clocks %d, hands %d, worst %.2f degrees off the tower"), Clocks, St.Hands, ClockErr));
    Report->SetNumberField(TEXT("wallClocks"), Clocks); Report->SetNumberField(TEXT("clockErrorDegrees"), ClockErr);
    // Signs and sound.
    UHouseBlackHallSubsystem* Look = W->GetSubsystem<UHouseBlackHallSubsystem>();
    const FString Sign = SI::SignKey(Id) ? FString(SI::SignKey(Id)) : FString::Printf(TEXT("CI_Gen%sSign"), *Id);
    const TObjectPtr<UMaterialInterface>* SignMat = Look ? Look->Materials.Find(FName(*Sign)) : nullptr;
    if (!SignMat || !St.Materials.Contains(SignMat->Get())) Problems.Add(FString::Printf(TEXT("sign %s not on the building"), *Sign));
    Report->SetStringField(TEXT("sign"), Sign);
    TArray<HouseCampusInteriorsImpl::FSoundSpot> Spots;
    if (Def->Sounds) Def->Sounds(Seed, Height, Spots);
    FString SoundCheck = TEXT("checked");
    if (Spots.Num() == 0) SoundCheck = TEXT("none specified");
    else if (!GEngine || !GEngine->UseSound()) SoundCheck = TEXT("skipped: no audio device (-nosound)");
    else if (FParse::Param(FCommandLine::Get(), TEXT("NoCampusSound"))) SoundCheck = TEXT("skipped: -NoCampusSound");
    else if (St.Sounds != Spots.Num() || St.PlayingSounds != Spots.Num()) Problems.Add(FString::Printf(TEXT("sounds %d started, %d playing, of %d"), St.Sounds, St.PlayingSounds, Spots.Num()));
    Report->SetNumberField(TEXT("sounds"), St.Sounds); Report->SetNumberField(TEXT("soundsPlaying"), St.PlayingSounds); Report->SetStringField(TEXT("soundCheck"), SoundCheck);
    // Windows into built rooms.
    if (bPlan)
    {
        int32 Real = 0;
        for (int32 Side = 0; Side < 4; ++Side)
        {
            TArray<FVector2D> Cols;
            HouseCampusInteriors::WindowColumns(Seed, Side, Cols);
            for (int32 K = 0; K < Def->OpenFloors; ++K) for (const FVector2D& C : Cols) Real += HouseCampusInteriors::WindowOpen(Seed, Side, K, C.X, C.Y) ? 1 : 0;
        }
        if (Real == 0) Problems.Add(TEXT("the plan opens no windows on the open floors"));
        Report->SetNumberField(TEXT("realWindows"), Real);
    }
    const bool bPass = Problems.Num() == 0;
    Report->SetBoolField(TEXT("passed"), bPass);
    TArray<TSharedPtr<FJsonValue>> P;
    for (const FString& Pr : Problems) P.Add(MakeShared<FJsonValueString>(Pr));
    Report->SetArrayField(TEXT("problems"), P);
    FString Json;
    FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
    FFileHelper::SaveStringToFile(Json, *(S.Dir / (Id + TEXT(".json"))));
    S.Summary += FString::Printf(TEXT("%s: %s tris=%d kit=%d props=%d floating=%d anchors=%d/%d doors=%d locked=%d reasons=%d clocks=%d clockErr=%.2f sounds=%d/%d%s\n"), *Id, bPass ? TEXT("PASS") : TEXT("FAIL"),
        St.Triangles, St.Decor, Props, Floating, FloorHits, Anchors.Num(), Doors, Locked, Reasons, Clocks, ClockErr, St.PlayingSounds, Spots.Num(), bPass ? TEXT("") : *(TEXT(" problems: ") + FString::Join(Problems, TEXT("; "))));
    UE_LOG(LogTemp, Display, TEXT("CAMPUS_SMOKE_%s_%s %s"), *Id, bPass ? TEXT("PASSED") : TEXT("FAILED"), *FString::Join(Problems, TEXT("; ")));
    S.Passed += bPass ? 1 : 0;
    ++S.Index;
    S.Phase = 0;
}
#else
void TickHouseCampusInteriorsSmoke(UWorld*) {}
#endif
