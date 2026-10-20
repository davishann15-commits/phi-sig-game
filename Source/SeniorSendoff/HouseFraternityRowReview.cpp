// Opt-in acceptance only. Standalone registration avoids changes to StoryCampaign/player movement.
#if !UE_BUILD_SHIPPING
#include "StoryCampaign.h"
#include "HouseFraternityRow.h"
#include "HouseExterior.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseCampusInteriors.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallMesh.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderMapTrees.h"
#include "HouseWestField.h"
#include "HouseWoodsDetail.h"
#include "HouseStreetScene.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Crc.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodyInstance.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StaticMeshResources.h"
#include "UnrealClient.h"

namespace HouseFraternityRowReviewImpl
{
constexpr uint32 ExpectedTreeCRC=0x0f3b2bb7;
const FDateTime ReviewLoadedUTC=FDateTime::UtcNow();
const TCHAR* const ISMNames[]={TEXT("FraternityRowCups"),TEXT("FraternityRowCans"),TEXT("FraternityRowLeavesBrown"),TEXT("FraternityRowLeavesGold"),
    TEXT("FraternityRowGrass"),TEXT("FraternityRowBrambles"),TEXT("FraternityRowSaplings"),TEXT("FraternityRowPaper")};
struct FRect{float L,R,F,B;bool Contains(FVector2D P)const{return P.X>=L&&P.X<=R&&P.Y>=F&&P.Y<=B;}};
const FRect Sheds[]={{-16680,-16180,3950,4330},{-14900,-14300,3920,4350},{-10500,-9750,3850,4380},
    {-8240,-7790,3890,4320},{-5140,-4700,3970,4350}};
const TArray<TArray<FVector2D>>& Paths()
{
    static const TArray<TArray<FVector2D>> P={
        {{-17400,85},{-17400,900},{-17100,1450},{-17100,3300},{-16900,4400}},
        {{-13900,-50},{-13900,900},{-13650,2300},{-13650,4400}},
        {{-10100,-20},{-10100,550},{-9700,1900},{-9350,3300},{-9350,4400}},
        {{-7600,2350},{-7600,2700},{-7400,3100},{-7400,3620},{-7500,4400}},
        {{-6300,300},{-6300,700},{-5500,900},{-5500,2850},{-5700,3750},{-5550,4400}}};return P;
}
struct FGoal{FVector2D P;FString Label;bool bShot=false;};
struct FState
{
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AStoryFirstPersonCharacter> Walker;
    TWeakObjectPtr<ACampusDoor> RearA,RearB;
    FDateTime StartedUTC;double Started=0,Step=0,Progress=0,LastFloor=0,FinishAt=0;
    int32 Phase=0,Yard=0,Leg=0,Reached=0,CompletedYards=0,Setups=0,FloorChecks=0,DoorOpens=0;
    float Best=MAX_flt;FVector Target;TArray<FGoal> Route;TArray<FString> Captures;
    FString Dir,Report,FeetCSV=TEXT("time,yard,leg,x,y,z,floor_z,gap\n");
};
FState S;
void Note(const FString& Line){S.Report+=Line+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("FRATERNITY_ROW_REVIEW %s"),*Line);}
void Finish(bool Pass,const FString& Why)
{
    if(S.Walker.IsValid())S.Walker->GetCharacterMovement()->StopMovementImmediately();
    S.Phase=-1;Note(FString::Printf(TEXT("%s yards=%d/5 setups=%d walkedGoals=%d floorChecks=%d rearDoorInteractions=%d %s"),
        Pass?TEXT("PASS"):TEXT("FAIL"),S.CompletedYards,S.Setups,S.Reached,S.FloorChecks,S.DoorOpens,*Why));
    FFileHelper::SaveStringToFile(S.Report,*(S.Dir/TEXT("result.txt")));
    FFileHelper::SaveStringToFile(S.FeetCSV,*(S.Dir/TEXT("feet.csv")));
    FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);
}
bool Prefix(const UPrimitiveComponent* P,const TCHAR* Name){return P&&P->GetName().StartsWith(Name);}
bool BodyReady(UPrimitiveComponent* P)
{const FBodyInstance* B=P?P->GetBodyInstance():nullptr;return P&&P->IsRegistered()&&P->GetCollisionEnabled()!=ECollisionEnabled::NoCollision&&B&&B->IsValidBodyInstance();}
bool Ground(UWorld* W,AActor* Ignore,FVector2D P,float Expected,float Tolerance,FHitResult* Out=nullptr)
{
    FHitResult H;const bool Hit=W->LineTraceSingleByChannel(H,FVector(P,Expected+80),FVector(P,Expected-90),ECC_Pawn,
        FCollisionQueryParams(SCENE_QUERY_STAT(FraternityRowGround),true,Ignore));if(Out)*Out=H;
    return Hit&&FMath::Abs(H.ImpactPoint.Z-Expected)<Tolerance&&H.ImpactNormal.Z>.70f;
}
bool OwnHit(UWorld* W,AActor* Ignore,FVector A,FVector B,const TCHAR* ComponentPrefix)
{
    // Isolate the permanent exterior: a normally loaded interior finish sits0.35cm above this structural floor.
    FCollisionQueryParams Q(SCENE_QUERY_STAT(FraternityRowSolid),true,Ignore);
    for(TActorIterator<AActor> It(W);It;++It)if(!It->IsA<AHouseExteriorDetail>())Q.AddIgnoredActor(*It);
    FHitResult H;return W->LineTraceSingleByChannel(H,A,B,ECC_Pawn,Q)
        &&Prefix(H.GetComponent(),ComponentPrefix)&&BodyReady(H.GetComponent());
}
float ShedFloor(const FRect& R)
{
    return FMath::Max(FMath::Max(HouseExteriorSite::GroundHeight(R.L,R.F),HouseExteriorSite::GroundHeight(R.R,R.F)),
        FMath::Max(HouseExteriorSite::GroundHeight(R.L,R.B),HouseExteriorSite::GroundHeight(R.R,R.B)))+.5f;
}
struct FTreePose {int32 Index=0;FTransform World=FTransform::Identity;bool bRead=false;};
struct FTreeBatch
{
    FString Name,MeshName,MeshPath,OwnerName,OwnerClass;int32 Lods=0,Triangles=0;
    bool bRegistered=false,bVisible=false,bHiddenInGame=false,bOwnerHidden=true,bRenderData=false,bOriginal=false;
    TArray<FTreePose> Poses;
    bool Eligible()const{return bRegistered&&bVisible&&!bHiddenInGame&&!bOwnerHidden&&bRenderData;}
};
bool ReadNumbers(const TSharedPtr<FJsonObject>& O,const TCHAR* Field,int32 Count,TArray<double>& Out)
{
    const TArray<TSharedPtr<FJsonValue>>* A=nullptr;Out.Reset();if(!O||!O->TryGetArrayField(Field,A)||A->Num()!=Count)return false;
    for(const auto& V:*A){double D=0;if(!V.IsValid()||!V->TryGetNumber(D)||!FMath::IsFinite(D))return false;Out.Add(D);}return true;
}
TArray<TSharedPtr<FJsonValue>> Numbers(std::initializer_list<double> Values)
{
    TArray<TSharedPtr<FJsonValue>> Out;for(double V:Values)Out.Add(MakeShared<FJsonValueNumber>(FMath::IsFinite(V)?V:0.));return Out;
}
bool TreeInstances(UWorld* W,TArray<FVector2D>& DrawnRoots)
{
    TArray<FTreeBatch> Batches;int32 Rows=0,BadReads=0,EligibleComponents=0;
    for(TActorIterator<AActor> It(W);It;++It)
    {
        TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(*It);
        for(auto* C:Parts)
        {
            const FString Name=C->GetName();if(!HouseCalderMapTrees::TreeBatch(Name))continue;
            FTreeBatch B;B.Name=Name;B.OwnerName=It->GetName();B.OwnerClass=It->GetClass()->GetPathName();
            const UStaticMesh* Mesh=C->GetStaticMesh();const auto* Data=Mesh?Mesh->GetRenderData():nullptr;
            B.MeshName=GetNameSafe(Mesh);B.MeshPath=Mesh?Mesh->GetPathName():TEXT("");B.Lods=Data?Data->LODResources.Num():0;
            B.Triangles=B.Lods>0?Data->LODResources[0].GetNumTriangles():0;B.bRenderData=Mesh&&Data&&B.Lods>0&&B.Triangles>0;
            B.bRegistered=C->IsRegistered();B.bVisible=C->IsVisible();B.bHiddenInGame=C->bHiddenInGame;B.bOwnerHidden=It->IsHidden();
            B.bOriginal=Name.StartsWith(TEXT("ExteriorTree"))||Name.StartsWith(TEXT("CampusFallTree"));EligibleComponents+=B.Eligible();
            for(int32 I=0;I<C->GetInstanceCount();++I)
            {
                FTreePose P;P.Index=I;P.bRead=C->GetInstanceTransform(I,P.World,true)&&!P.World.ContainsNaN()&&P.World.GetRotation().IsNormalized();BadReads+=!P.bRead;++Rows;
                if(P.bRead&&B.bOriginal&&B.Eligible()&&P.World.GetScale3D().GetAbs().GetMin()>=.01f)DrawnRoots.Add(FVector2D(P.World.GetLocation()));
                B.Poses.Add(P);
            }
            Batches.Add(MoveTemp(B));
        }
    }
    Batches.Sort([](const FTreeBatch& A,const FTreeBatch& B)
    {if(A.Name!=B.Name)return A.Name<B.Name;if(A.MeshPath!=B.MeshPath)return A.MeshPath<B.MeshPath;
        if(A.OwnerClass!=B.OwnerClass)return A.OwnerClass<B.OwnerClass;return A.OwnerName<B.OwnerName;});
    auto Root=MakeShared<FJsonObject>();Root->SetStringField(TEXT("schema"),TEXT("FraternityRowTreeInstancesV2"));
    Root->SetStringField(TEXT("review_loaded_utc"),ReviewLoadedUTC.ToIso8601());Root->SetStringField(TEXT("captured_utc"),FDateTime::UtcNow().ToIso8601());
    Root->SetBoolField(TEXT("upgrade_enabled"),HouseFraternityRow::Enabled());Root->SetNumberField(TEXT("component_count"),Batches.Num());Root->SetNumberField(TEXT("instance_count"),Rows);
    FString Source;const FString SourceFile=FPaths::ProjectSavedDir()/TEXT("WoodsPassReview/original_trees.csv");
    const bool SourceLoaded=FFileHelper::LoadFileToString(Source,*SourceFile)&&Source.StartsWith(TEXT("batch,index,transform\n"));
    const bool Fresh=IFileManager::Get().GetTimeStamp(*SourceFile)>=S.StartedUTC-FTimespan::FromSeconds(2);
    const uint32 SourceCRC=SourceLoaded?FCrc::MemCrc32(*Source,Source.Len()*sizeof(TCHAR)):0;
    Root->SetStringField(TEXT("source_crc"),FString::Printf(TEXT("%08x"),SourceCRC));Root->SetBoolField(TEXT("source_snapshot_fresh"),Fresh);
    const bool Captured=Batches.Num()>0&&Rows>0&&BadReads==0&&SourceLoaded&&Fresh&&SourceCRC==ExpectedTreeCRC;
    Root->SetBoolField(TEXT("capture_valid"),Captured);
    TArray<TSharedPtr<FJsonValue>> Components;
    for(const FTreeBatch& B:Batches)
    {
        auto C=MakeShared<FJsonObject>();C->SetStringField(TEXT("component"),B.Name);C->SetStringField(TEXT("mesh_name"),B.MeshName);C->SetStringField(TEXT("mesh_path"),B.MeshPath);
        C->SetStringField(TEXT("owner_name"),B.OwnerName);C->SetStringField(TEXT("owner_class"),B.OwnerClass);C->SetNumberField(TEXT("lods"),B.Lods);C->SetNumberField(TEXT("triangles_lod0"),B.Triangles);
        C->SetBoolField(TEXT("registered"),B.bRegistered);C->SetBoolField(TEXT("visible"),B.bVisible);C->SetBoolField(TEXT("hidden_in_game"),B.bHiddenInGame);
        C->SetBoolField(TEXT("owner_hidden"),B.bOwnerHidden);C->SetBoolField(TEXT("valid_render_data"),B.bRenderData);C->SetBoolField(TEXT("eligible_rendered_original"),B.bOriginal&&B.Eligible());
        TArray<TSharedPtr<FJsonValue>> Instances;
        for(const FTreePose& P:B.Poses)
        {
            auto O=MakeShared<FJsonObject>();const FVector L=P.World.GetLocation(),Scale=P.World.GetScale3D();const FQuat Q=P.World.GetRotation();
            O->SetNumberField(TEXT("index"),P.Index);O->SetBoolField(TEXT("read_valid"),P.bRead);
            O->SetArrayField(TEXT("location"),Numbers({L.X,L.Y,L.Z}));O->SetArrayField(TEXT("rotation_xyzw"),Numbers({Q.X,Q.Y,Q.Z,Q.W}));O->SetArrayField(TEXT("scale"),Numbers({Scale.X,Scale.Y,Scale.Z}));
            if(!P.bRead)O->SetStringField(TEXT("invalid_raw_transform"),P.World.ToString());Instances.Add(MakeShared<FJsonValueObject>(O));
        }
        C->SetArrayField(TEXT("instances"),Instances);Components.Add(MakeShared<FJsonValueObject>(C));
    }
    Root->SetArrayField(TEXT("components"),Components);
    FString BaselinePath;const bool ParsedBaselineArgument=FParse::Value(FCommandLine::Get(),TEXT("FraternityRowTreeBaseline="),BaselinePath,false);
    const bool Requested=ParsedBaselineArgument||FParse::Param(FCommandLine::Get(),TEXT("FraternityRowTreeBaseline"))||FString(FCommandLine::Get()).Contains(TEXT("FraternityRowTreeBaseline="));
    FString ActualPath=S.Dir/TEXT("actual_tree_instances.json");TSharedPtr<FJsonObject> Baseline;FString Problem;FDateTime BaselineStamp=FDateTime::MinValue();
    if(Requested)
    {
        BaselineStamp=IFileManager::Get().GetTimeStamp(*BaselinePath);
        FString Data;
        if(BaselinePath.IsEmpty()||!FFileHelper::LoadFileToString(Data,*BaselinePath)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Data),Baseline)||!Baseline)
            Problem=TEXT("supplied baseline is missing/empty/unreadable/malformed");
        else if(BaselineStamp==FDateTime::MinValue()||BaselineStamp>=ReviewLoadedUTC)
            Problem=TEXT("baseline file timestamp must be strictly before current review-module load");
        FString A=FPaths::ConvertRelativePathToFull(ActualPath),B=FPaths::ConvertRelativePathToFull(BaselinePath);FPaths::NormalizeFilename(A);FPaths::NormalizeFilename(B);FPaths::CollapseRelativeDirectories(A);FPaths::CollapseRelativeDirectories(B);
        if(A.Equals(B,ESearchCase::IgnoreCase)){Problem=TEXT("baseline must be a prior-run artifact, not the current output path");ActualPath=S.Dir/TEXT("actual_tree_instances_current.json");}
    }
    // Always export actual state, including hidden/invalid components. Load the reference first so it cannot be replaced
    // by this output before comparison. Transform arrays retain double precision; invalid numerics are flagged explicitly.
    FString JSON;const bool Written=FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&JSON))&&FFileHelper::SaveStringToFile(JSON,*ActualPath);
    Note(FString::Printf(TEXT("TREE_INSTANCES %s components=%d eligibleComponents=%d rows=%d badTransformReads=%d path=%s"),Written&&Captured?TEXT("PASS"):TEXT("FAIL"),Batches.Num(),EligibleComponents,Rows,BadReads,*ActualPath));
    if(!Requested)
    {
        if(HouseFraternityRow::Enabled())
        {Note(FString::Printf(TEXT("TREE_BASELINE FAIL upgraded scene requires -FraternityRowTreeBaseline=<prior legacy capture>; actualSnapshotWritten=%d"),int(Written)));return false;}
        Note(TEXT("TREE_BASELINE LEGACY_CAPTURE_ONLY; upgrade disabled, rendered TRS/mesh equivalence is not claimed"));return Written&&Captured;
    }
    int32 Compared=0;double MaxPosition=0,MaxAngle=0,MaxScale=0;
    if(Problem.IsEmpty())
    {
        FString Schema,CRC,LoadedText,CapturedText;bool Valid=false,SourceFresh=false,Upgraded=true;double Count=0,InstanceCount=0;
        FDateTime LoadedUTC,CapturedUTC;const TArray<TSharedPtr<FJsonValue>>* BaseComponents=nullptr;
        if(!Baseline->TryGetBoolField(TEXT("upgrade_enabled"),Upgraded)||Upgraded)
            Problem=TEXT("baseline must explicitly record upgrade_enabled=false from the legacy control");
        else if(!Baseline->TryGetStringField(TEXT("review_loaded_utc"),LoadedText)||!FDateTime::ParseIso8601(*LoadedText,LoadedUTC)
            ||!Baseline->TryGetStringField(TEXT("captured_utc"),CapturedText)||!FDateTime::ParseIso8601(*CapturedText,CapturedUTC)
            ||LoadedText!=LoadedUTC.ToIso8601()||CapturedText!=CapturedUTC.ToIso8601()
            ||LoadedUTC==FDateTime::MinValue()||LoadedUTC>CapturedUTC||CapturedUTC>=ReviewLoadedUTC)
            Problem=TEXT("baseline load/capture UTC must identify an earlier run, captured after its own module load and before this one");
        else if(!Baseline->TryGetStringField(TEXT("schema"),Schema)||Schema!=TEXT("FraternityRowTreeInstancesV2")||!Baseline->TryGetBoolField(TEXT("capture_valid"),Valid)||!Valid
            ||!Baseline->TryGetStringField(TEXT("source_crc"),CRC)||CRC!=TEXT("0f3b2bb7")||!Baseline->TryGetBoolField(TEXT("source_snapshot_fresh"),SourceFresh)||!SourceFresh
            ||!Baseline->TryGetNumberField(TEXT("component_count"),Count)||Count!=Batches.Num()||!Baseline->TryGetNumberField(TEXT("instance_count"),InstanceCount)||InstanceCount!=Rows
            ||!Baseline->TryGetArrayField(TEXT("components"),BaseComponents)||BaseComponents->Num()!=Batches.Num())Problem=TEXT("schema/sourceCRC/freshness/component/row count mismatch");
        if(Problem.IsEmpty())for(int32 I=0;I<Batches.Num();++I)
        {
            const FTreeBatch& B=Batches[I];const auto& ComponentValue=(*BaseComponents)[I];
            const TSharedPtr<FJsonObject> O=ComponentValue.IsValid()&&ComponentValue->Type==EJson::Object?ComponentValue->AsObject():nullptr;if(!O){Problem=TEXT("malformed component object");break;}
            auto Str=[&](const TCHAR* Key,const FString& Expected){FString V;return O->TryGetStringField(Key,V)&&V==Expected;};
            auto Bool=[&](const TCHAR* Key,bool Expected){bool V=false;return O->TryGetBoolField(Key,V)&&V==Expected;};
            auto Number=[&](const TCHAR* Key,int32 Expected){double V=0;return O->TryGetNumberField(Key,V)&&V==Expected;};
            const TArray<TSharedPtr<FJsonValue>>* BasePoses=nullptr;
            if(!Str(TEXT("component"),B.Name)||!Str(TEXT("mesh_name"),B.MeshName)||!Str(TEXT("mesh_path"),B.MeshPath)||!Str(TEXT("owner_name"),B.OwnerName)||!Str(TEXT("owner_class"),B.OwnerClass)
                ||!Bool(TEXT("registered"),B.bRegistered)||!Bool(TEXT("visible"),B.bVisible)||!Bool(TEXT("hidden_in_game"),B.bHiddenInGame)||!Bool(TEXT("owner_hidden"),B.bOwnerHidden)||!Bool(TEXT("valid_render_data"),B.bRenderData)
                ||!Number(TEXT("lods"),B.Lods)||!Number(TEXT("triangles_lod0"),B.Triangles)||!O->TryGetArrayField(TEXT("instances"),BasePoses)||BasePoses->Num()!=B.Poses.Num())
            {Problem=TEXT("component/mesh/owner/render flags/LOD/instance count mismatch at ")+B.Name;break;}
            for(int32 J=0;J<B.Poses.Num();++J)
            {
                const auto& PoseValue=(*BasePoses)[J];const TSharedPtr<FJsonObject> P=PoseValue.IsValid()&&PoseValue->Type==EJson::Object?PoseValue->AsObject():nullptr;
                TArray<double> L,Q,Scale;double Index=-1;bool Read=false;
                if(!P||!P->TryGetNumberField(TEXT("index"),Index)||Index!=J||!P->TryGetBoolField(TEXT("read_valid"),Read)||!Read||!B.Poses[J].bRead
                    ||!ReadNumbers(P,TEXT("location"),3,L)||!ReadNumbers(P,TEXT("rotation_xyzw"),4,Q)||!ReadNumbers(P,TEXT("scale"),3,Scale))
                {Problem=TEXT("invalid/missing full transform at ")+B.Name+FString::Printf(TEXT("[%d]"),J);break;}
                const FQuat Rotation(Q[0],Q[1],Q[2],Q[3]);const FTransform& Current=B.Poses[J].World;
                if(!Rotation.IsNormalized()){Problem=TEXT("baseline quaternion is not normalized at ")+B.Name;break;}
                const double Position=FVector::Dist(Current.GetLocation(),FVector(L[0],L[1],L[2]));
                // The chord ratio is stable at identical rotations; acos(dot) amplifies normalization roundoff.
                const FQuat A=Current.GetRotation().GetNormalized(),Other=Rotation.GetNormalized();
                const double Sign=(A.X*Other.X+A.Y*Other.Y+A.Z*Other.Z+A.W*Other.W)<0?-1.:1.;
                const FQuat Aligned(Other.X*Sign,Other.Y*Sign,Other.Z*Sign,Other.W*Sign);
                const double Angle=FMath::RadiansToDegrees(4.*FMath::Atan2((A-Aligned).Size(),(A+Aligned).Size()));
                const double Size=(Current.GetScale3D()-FVector(Scale[0],Scale[1],Scale[2])).GetAbsMax();
                MaxPosition=FMath::Max(MaxPosition,Position);MaxAngle=FMath::Max(MaxAngle,Angle);MaxScale=FMath::Max(MaxScale,Size);++Compared;
                if(Position>.001||Angle>.0001||Size>.000001)
                {Problem=TEXT("rendered TRS changed at ")+B.Name+FString::Printf(TEXT("[%d] positionCm=%.9g angleDegrees=%.9g scale=%.9g"),J,Position,Angle,Size);break;}
            }
            if(!Problem.IsEmpty())break;
        }
    }
    const bool Matched=Problem.IsEmpty()&&Compared==Rows&&Written&&Captured;
    Note(FString::Printf(TEXT("TREE_BASELINE %s compared=%d/%d maxPositionCm=%.9g toleranceCm=.001 maxRotationDegrees=%.9g toleranceDegrees=.0001 maxScale=%.9g toleranceScale=.000001 file=%s fileUTC=%s currentReviewLoadedUTC=%s %s"),
        Matched?TEXT("PASS"):TEXT("FAIL"),Compared,Rows,MaxPosition,MaxAngle,MaxScale,*BaselinePath,*BaselineStamp.ToIso8601(),*ReviewLoadedUTC.ToIso8601(),*Problem));return Matched;
}
// Actual rendered mesh bounds, not root clearance, protect the relocated Queen Anne.
bool CanopyEnvelope(UWorld* W)
{
    if(!HouseFraternityRow::Enabled())return true;
    const auto& H=HouseFraternityRow::Houses()[4];
    const FBox2D Reserved({H.L-160,H.F-290},{H.R+50,H.B+408});
    int32 Tested=0,Overlaps=0,BadReads=0;
    for(TActorIterator<AActor> It(W);It;++It)
    {
        if(It->IsHidden())continue;
        TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(*It);
        for(auto* C:Parts)
        {
            if(!HouseCalderMapTrees::TreeBatch(C->GetName())&&C->GetFName()!=FName(TEXT("FraternityRowSaplings")))continue;
            if(!C->IsRegistered()||!C->IsVisible()||C->bHiddenInGame)continue;
            const UStaticMesh* Mesh=C->GetStaticMesh();if(!Mesh){++BadReads;continue;}
            for(int32 I=0;I<C->GetInstanceCount();++I)
            {
                FTransform T;if(!C->GetInstanceTransform(I,T,true)||T.ContainsNaN()){++BadReads;continue;}
                const FBox B=Mesh->GetBoundingBox().TransformBy(T);++Tested;
                if(B.Min.X<Reserved.Max.X&&B.Max.X>Reserved.Min.X&&B.Min.Y<Reserved.Max.Y&&B.Max.Y>Reserved.Min.Y)
                {++Overlaps;Note(FString::Printf(TEXT("CANOPY_ENVELOPE overlap component=%s instance=%d bounds=%s"),*C->GetName(),I,*B.ToString()));}
            }
        }
    }
    const bool Good=Tested>=1600&&Overlaps==0&&BadReads==0;
    Note(FString::Printf(TEXT("CANOPY_ENVELOPE %s tested=%d overlaps=%d badReads=%d actualMeshBounds=1 originalTreesChanged=0"),Good?TEXT("PASS"):TEXT("FAIL"),Tested,Overlaps,BadReads));
    return Good;
}
bool TreeCRC(const TArray<FVector2D>& DrawnRoots,int32& Hidden)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("WoodsPassReview")))
    {Note(TEXT("CHECK FAIL treeCRC requires -WoodsPassReview from this same unique run directory"));return false;}
    const FString File=FPaths::ProjectSavedDir()/TEXT("WoodsPassReview/original_trees.csv");FString Data;
    const FDateTime Stamp=IFileManager::Get().GetTimeStamp(*File);
    const bool Fresh=Stamp>=S.StartedUTC-FTimespan::FromSeconds(2);
    if(!Fresh||!FFileHelper::LoadFileToString(Data,*File)||!Data.StartsWith(TEXT("batch,index,transform\n")))
    {Note(TEXT("CHECK FAIL original tree snapshot missing/stale/malformed: ")+File);return false;}
    const uint32 CRC=FCrc::MemCrc32(*Data,Data.Len()*sizeof(TCHAR));int32 Original=0;Hidden=0;bool Parsed=true;
    TArray<FString> Lines;Data.ParseIntoArrayLines(Lines,false);
    for(const FString& Line:Lines)
    {
        FString Batch,Rest,Index,Pose,XYZ,Other;if(!Line.Split(TEXT(","),&Batch,&Rest)||(Batch!=TEXT("Big")&&Batch!=TEXT("Small")))continue;
        if(!Rest.Split(TEXT(","),&Index,&Pose)||!Pose.Split(TEXT("|"),&XYZ,&Other)){Parsed=false;continue;}
        TArray<FString> Values;XYZ.ParseIntoArray(Values,TEXT(","),false);if(Values.Num()!=3){Parsed=false;continue;}
        const FVector2D P(FCString::Atod(*Values[0]),FCString::Atod(*Values[1]));++Original;bool Drawn=false;
        for(const FVector2D& Root:DrawnRoots)if(FVector2D::Distance(P,Root)<.5){Drawn=true;break;}
        Hidden+=!Drawn;
    }
    const bool Good=CRC==ExpectedTreeCRC&&Parsed&&Original>0&&Hidden<=40;
    Note(FString::Printf(TEXT("CHECK %s originalTreeCRC=%08x expected=%08x freshSnapshot=%d originalAB=%d renderedRootEntries=%d missingOrHiddenAB=%d maxMissingOrHiddenAB=40"),
        Good?TEXT("PASS"):TEXT("FAIL"),CRC,ExpectedTreeCRC,int(Fresh),Original,DrawnRoots.Num(),Hidden));
    return Good;
}
bool Scene(UWorld* W,AStoryFirstPersonCharacter* Pawn)
{
    const bool On=HouseFraternityRow::Enabled(),Lit=HouseFraternityRow::LightsEnabled();AHouseExteriorDetail* E=nullptr;
    TArray<FVector2D> DrawnRoots;int32 HiddenTrees=0;const bool ActualTrees=TreeInstances(W,DrawnRoots),SourceTrees=TreeCRC(DrawnRoots,HiddenTrees);
    for(TActorIterator<AHouseExteriorDetail> It(W);It;++It){if(E){Note(TEXT("CHECK FAIL multiple exterior actors"));return false;}E=*It;}
    auto* Look=W->GetSubsystem<UHouseBlackHallSubsystem>();if(!E||!Look){Note(TEXT("CHECK FAIL missing exterior/look"));return false;}
    int32 Structural=0,YardSolid=0,Decor=0,BadSolid=0,BadDecor=0,OldParkingParts=0,LotCars=0,OldPoles=0,AsphaltTriangles=0;
    int32 Batches=0,Instances=0,BadISM=0,ExteriorLights=0,ShadowLights=0,InteriorLights=0;
    TSet<FName> FoundBatches;UProceduralMeshComponent* Fence=nullptr;
    TArray<UPointLightComponent*> ActualExteriorLights;TArray<FVector> ActualRowGlowVertices;
    const FRect OldLot{-7200,-3700,-2500,-300},Apron{-5800,-5200,-4149,-2500};
    const FVector2D RemovedPoles[]={{-7160,-2600},{-3740,-2530}};
    for(TActorIterator<AActor> It(W);It;++It)
    {
        TInlineComponentArray<UPrimitiveComponent*> Parts(*It);
        for(UPrimitiveComponent* P:Parts)
        {
            const FString N=P->GetName();
            if(Prefix(P,TEXT("FraternityRowStructure"))){++Structural;BadSolid+=!BodyReady(P);}
            if(Prefix(P,TEXT("FraternityRowYardsSolid"))){++YardSolid;BadSolid+=!BodyReady(P)||P->CastShadow||P->bAffectDistanceFieldLighting||P->bAffectDynamicIndirectLighting;}
            if(Prefix(P,TEXT("FraternityRowDetail"))||Prefix(P,TEXT("FraternityRowGlow"))||Prefix(P,TEXT("FraternityRowYardsDecor"))||Prefix(P,TEXT("FraternityRowYardsGlow")))
            {++Decor;BadDecor+=P->GetCollisionEnabled()!=ECollisionEnabled::NoCollision||P->CastShadow||P->bAffectDistanceFieldLighting||P->bAffectDynamicIndirectLighting;}
            if(N.StartsWith(TEXT("ExteriorParkingLot"))||N.StartsWith(TEXT("ExteriorLotProps"))||N.StartsWith(TEXT("ExteriorBushParking")))++OldParkingParts;
            if(N==TEXT("ExteriorWestFieldFence"))Fence=Cast<UProceduralMeshComponent>(P);
            if(auto* ISM=Cast<UInstancedStaticMeshComponent>(P))
            {
                const bool RowBatch=N.StartsWith(TEXT("FraternityRow"));
                if(RowBatch)
                {
                    ++Batches;Instances+=ISM->GetInstanceCount();FoundBatches.Add(ISM->GetFName());
                    const UStaticMesh* M=ISM->GetStaticMesh();const auto* Data=M?M->GetRenderData():nullptr;
                    BadISM+=Cast<UHierarchicalInstancedStaticMeshComponent>(ISM)!=nullptr||ISM->GetCollisionEnabled()!=ECollisionEnabled::NoCollision
                        ||ISM->CastShadow||ISM->bAffectDynamicIndirectLighting||ISM->bAffectDistanceFieldLighting||!Data||Data->LODResources.Num()==0
                        ||(Data&&Data->LODResources.Num()>0&&Data->LODResources[0].GetNumTriangles()==0)||ISM->InstanceEndCullDistance<=0||ISM->GetInstanceCount()==0;
                }
                if(N.StartsWith(TEXT("ExteriorCar"))||N==TEXT("ExteriorPoleLot")||N==TEXT("ExteriorPole"))for(int32 J=0;J<ISM->GetInstanceCount();++J)
                {
                    FTransform T;ISM->GetInstanceTransform(J,T,true);const FVector2D Pos(T.GetLocation());
                    if(N.StartsWith(TEXT("ExteriorCar")))
                    {
                        const UStaticMesh* Mesh=ISM->GetStaticMesh();const FBox Bounds=Mesh?Mesh->GetBoundingBox().TransformBy(T):FBox(T.GetLocation(),T.GetLocation());
                        LotCars+=Bounds.Min.X<OldLot.R&&Bounds.Max.X>OldLot.L&&Bounds.Min.Y<OldLot.B&&Bounds.Max.Y>OldLot.F;
                    }
                    if(N.StartsWith(TEXT("ExteriorPole")))for(FVector2D Pole:RemovedPoles)OldPoles+=FVector2D::Distance(Pos,Pole)<20;
                }
            }
            auto* PM=Cast<UProceduralMeshComponent>(P);if(!PM)continue;
            for(int32 K=0;K<PM->GetNumSections();++K)
            {
                auto* Section=PM->GetProcMeshSection(K);if(!Section)continue;const auto* Mat=PM->GetMaterial(K);
                if(N.StartsWith(TEXT("FraternityRowGlow"))||N.StartsWith(TEXT("FraternityRowYardsGlow")))
                    for(const FProcMeshVertex& V:Section->ProcVertexBuffer)ActualRowGlowVertices.Add(PM->GetComponentTransform().TransformPosition(V.Position));
                const auto A=Look->Materials.FindRef(TEXT("EX_Asphalt")),SA=Look->Materials.FindRef(TEXT("EX_StreetAsphalt"));
                const bool Asphalt=(A&&Mat==A)||(SA&&Mat==SA);
                if(!Asphalt)continue;
                for(int32 J=0;J+2<Section->ProcIndexBuffer.Num();J+=3)
                {
                    FBox2D B(ForceInit);for(int32 V=0;V<3;++V)B+=FVector2D(PM->GetComponentTransform().TransformPosition(Section->ProcVertexBuffer[Section->ProcIndexBuffer[J+V]].Position));
                    // AABB intersection, including triangles crossing the old lot's edges; road ends atY-4150.
                    const auto Intersects=[&](FRect R){return B.Min.X<R.R&&B.Max.X>R.L&&B.Min.Y<R.B&&B.Max.Y>R.F;};
                    AsphaltTriangles+=Intersects(OldLot)||Intersects(Apron);
                }
            }
        }
        TInlineComponentArray<UPointLightComponent*> Lights(*It);
        if(*It==E||It->GetOwner()==E)for(auto* L:Lights){++ExteriorLights;ShadowLights+=L->CastShadows;ActualExteriorLights.Add(L);}
        if(It->IsA<AHouseCampusInteriors>())InteriorLights+=Lights.Num();
    }
    bool ISMComplete=On?Batches==8&&Instances>0:Batches==0&&Instances==0;
    if(On)for(const TCHAR* Name:ISMNames)ISMComplete&=FoundBatches.Contains(FName(Name));
    const bool Geometry=On?HouseFraternityRow::Houses().Num()==5&&Structural>0&&YardSolid>0&&Decor>0&&BadSolid==0&&BadDecor==0:Structural==0&&YardSolid==0&&Decor==0;
    const bool Parking=On?OldParkingParts==0&&LotCars==0&&OldPoles==0&&AsphaltTriangles==0:OldParkingParts>0&&LotCars>=3&&OldPoles==2&&AsphaltTriangles>0;
    int32 ValidRowLights=0,OldParkingLightHeads=0,LightsOnActualGlow=0;
    for(const auto& H:HouseFraternityRow::Houses())if(H.Index>=2)
    {
        const FVector Porch=HouseFraternityRow::PorchLight(H),Yard=HouseFraternityRow::YardLight(H);
        for(int32 Which=0;Which<2;++Which)
        {int32 Found=0;for(const auto* L:ActualExteriorLights)if(FVector::Dist(L->GetComponentLocation(),Which==0?Porch:Yard)<.5f)
            Found+=!L->CastShadows&&FMath::Abs(L->AttenuationRadius-(Which==0?700:800))<.1f&&FMath::Abs(L->Intensity-(Which==0?650:500))<.1f;
            ValidRowLights+=Found==1;bool AtGlow=false;
            for(const FVector& V:ActualRowGlowVertices)if(FVector::Dist(V,Which==0?Porch:Yard)<12){AtGlow=true;break;}
            LightsOnActualGlow+=AtGlow;}
    }
    for(int32 I=0;I<2;++I)
    {const FVector2D P=RemovedPoles[I];const FVector Head=FVector(P,HouseExteriorSite::GroundHeight(P.X,P.Y))+FRotator(0,I==0?0:180,0).RotateVector(HouseStreetScene::LensOffset());
        for(const auto* L:ActualExteriorLights)OldParkingLightHeads+=FVector::Dist(L->GetComponentLocation(),Head)<.5f;}
    const bool LightBudget=ExteriorLights==HouseFraternityRow::ExteriorLightBudget()&&ShadowLights==0&&InteriorLights==6&&ValidRowLights==(Lit?6:0)&&OldParkingLightHeads==(On?0:2)&&LightsOnActualGlow==(On?6:0);
    const bool Trees=ActualTrees&&SourceTrees&&CanopyEnvelope(W);
    Note(FString::Printf(TEXT("CHECK %s enabled=%d structureComponents=%d yardSolidComponents=%d decorComponents=%d badCookedSolid=%d badDecor=%d"),Geometry?TEXT("PASS"):TEXT("FAIL"),int(On),Structural,YardSolid,Decor,BadSolid,BadDecor));
    Note(FString::Printf(TEXT("CHECK %s plainISMBatches=%d/8 actualInstances=%d badISM=%d originalHiddenAB=%d maxHiddenAB=40"),ISMComplete&&BadISM==0&&HiddenTrees<=40?TEXT("PASS"):TEXT("FAIL"),Batches,Instances,BadISM,HiddenTrees));
    Note(FString::Printf(TEXT("CHECK %s oldParkingParts=%d oldLotCars=%d oldParkingPoles=%d oldLotAsphaltTriangles=%d featureExpected=%s"),Parking?TEXT("PASS"):TEXT("FAIL"),OldParkingParts,LotCars,OldPoles,AsphaltTriangles,On?TEXT("absent"):TEXT("legacy")));
    Note(FString::Printf(TEXT("CHECK %s actualExteriorPointSpotLights=%d expected=%d shadowCasting=%d interiorPool=%d expectedPool=6 exactNewRowLightSpecs=%d expectedNew=%d lightsOnActualGlow=%d oldParkingHeads=%d expectedOld=%d"),LightBudget?TEXT("PASS"):TEXT("FAIL"),ExteriorLights,HouseFraternityRow::ExteriorLightBudget(),ShadowLights,InteriorLights,ValidRowLights,Lit?6:0,LightsOnActualGlow,OldParkingLightHeads,On?0:2));

    int32 HouseFloors=0,HouseWalls=0,ShedFloors=0,ShedWalls=0;
    if(On)
    {
        for(const auto& H:HouseFraternityRow::Houses())
        {
            HouseFloors+=OwnHit(W,Pawn,{H.X(),H.B-150,H.Floor+40},{H.X(),H.B-150,H.Floor-30},TEXT("FraternityRowStructure"));
            // Below the lowest45cm old-house window sill; opaque wall, not the glass collision pane.
            HouseWalls+=OwnHit(W,Pawn,{H.L-70,H.Y(),H.Floor+20},{H.L+65,H.Y(),H.Floor+20},TEXT("FraternityRowStructure"));
            HouseWalls+=OwnHit(W,Pawn,{H.R+70,H.Y(),H.Floor+20},{H.R-65,H.Y(),H.Floor+20},TEXT("FraternityRowStructure"));
            HouseWalls+=OwnHit(W,Pawn,{H.L+400,H.F-70,H.Floor+20},{H.L+400,H.F+65,H.Floor+20},TEXT("FraternityRowStructure"));
            HouseWalls+=OwnHit(W,Pawn,{H.L+400,H.B+70,H.Floor+20},{H.L+400,H.B-65,H.Floor+20},TEXT("FraternityRowStructure"));
        }
        for(const FRect& R:Sheds)
        {
            const float Z=ShedFloor(R),C=(R.L+R.R)*.5f,Y=(R.F+R.B)*.5f;
            ShedFloors+=OwnHit(W,Pawn,{C,R.F+130,Z+35},{C,R.F+130,Z-20},TEXT("FraternityRowYardsSolid"));
            ShedWalls+=OwnHit(W,Pawn,{R.L-50,Y,Z+70},{R.L+40,Y,Z+70},TEXT("FraternityRowYardsSolid"));
        }
    }
    const bool Physical=!On||(HouseFloors==5&&HouseWalls==20&&ShedFloors==5&&ShedWalls==5);
    Note(FString::Printf(TEXT("CHECK %s actualOwnHouseFloors=%d/5 ownHouseWalls=%d/20 ownShedFloors=%d/5 ownShedWalls=%d/5 legacyMode=%d"),Physical?TEXT("PASS"):TEXT("FAIL"),HouseFloors,HouseWalls,ShedFloors,ShedWalls,int(!On)));
    int32 DividerBlocks=0,DividerGaps=0,WorldGates=0;
    if(On)
    {
        FCollisionQueryParams DQ(SCENE_QUERY_STAT(FraternityRowDivider),true,Pawn),AllQ(SCENE_QUERY_STAT(FraternityRowSideGates),true,Pawn);
        for(TActorIterator<AActor> It(W);It;++It)if(*It!=E)DQ.AddIgnoredActor(*It);
        TInlineComponentArray<UPrimitiveComponent*> Parts(E);for(auto* P:Parts)if(!Prefix(P,TEXT("FraternityRowYardsSolid")))DQ.AddIgnoredComponent(P);
        const auto Capsule=FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        const float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();FHitResult Hit;
        for(FVector2D P:{FVector2D(-15750,2000),FVector2D(-12000,2000),FVector2D(-8500,2000),FVector2D(-6700,2600)})
        {const float Z=HouseExteriorSite::GroundHeight(P.X,P.Y)+Half+3;
            DividerBlocks+=W->SweepSingleByChannel(Hit,{P.X-150,P.Y,Z},{P.X+150,P.Y,Z},FQuat::Identity,ECC_Pawn,Capsule,DQ)&&Prefix(Hit.GetComponent(),TEXT("FraternityRowYardsSolid"));}
        // Deliberate clear crossing lines: front of the Tudor porch, around the preserved maple, and below its chair.
        for(FVector2D P:{FVector2D(-15750,925),FVector2D(-15750,3325),FVector2D(-12000,925),FVector2D(-12000,3325),
            FVector2D(-8500,100),FVector2D(-8500,3500),FVector2D(-6700,1900),FVector2D(-6700,3330)})
        {const float Z=HouseExteriorSite::GroundHeight(P.X,P.Y)+Half+3;
            DividerGaps+=!W->SweepSingleByChannel(Hit,{P.X-150,P.Y,Z},{P.X+150,P.Y,Z},FQuat::Identity,ECC_Pawn,Capsule,DQ);
            WorldGates+=!W->SweepSingleByChannel(Hit,{P.X-150,P.Y,Z},{P.X+150,P.Y,Z},FQuat::Identity,ECC_Pawn,Capsule,AllQ);}
    }
    const bool Dividers=!On||(DividerBlocks==4&&DividerGaps==8&&WorldGates==8);
    Note(FString::Printf(TEXT("CHECK %s dividerBarriers=%d/4 intrinsicDividerOpenings=%d/8 fullWorldSideGateCapsuleCrossings=%d/8 legacyMode=%d"),Dividers?TEXT("PASS"):TEXT("FAIL"),DividerBlocks,DividerGaps,WorldGates,int(!On)));
    int32 Plateau=0,Connector=0;
    for(float X:{-17500.f,-12500.f,-7000.f,-4400.f})for(float Y:{2100.f,2800.f,3700.f})Plateau+=Ground(W,Pawn,{X,Y},HouseWestField::MoundTop,6);
    for(FVector P:{FVector(-4320,-195,-57),FVector(-3700,-195,-57),FVector(-3480,-195,-57)})Connector+=Ground(W,Pawn,FVector2D(P),P.Z,3);
    FCollisionQueryParams FQ(SCENE_QUERY_STAT(FraternityRowFence),true,Pawn);for(TActorIterator<AActor> It(W);It;++It)if(*It!=E)FQ.AddIgnoredActor(*It);
    TInlineComponentArray<UPrimitiveComponent*> EP(E);for(auto* P:EP)if(P!=Fence)FQ.AddIgnoredComponent(P);
    const float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const auto Shape=FCollisionShape::MakeCapsule(Radius,Half);FHitResult H;int32 FenceBlocks=0;
    for(float X:{-18800.f,-13000.f,-9500.f,-8100.f,-6000.f,-4300.f})
    {const float Z=HouseWoods::Height(X,HouseWestField::FenceY)+Half+2;
        FenceBlocks+=W->SweepSingleByChannel(H,{X,HouseWestField::FenceY-160,Z},{X,HouseWestField::FenceY+160,Z},FQuat::Identity,ECC_Pawn,Shape,FQ)&&H.GetComponent()==Fence;}
    const float GX=(HouseWestField::GateWest+HouseWestField::GateEast)*.5f,GZ=HouseWoods::Height(GX,HouseWestField::FenceY)+Half+3;
    const bool GateClear=!W->SweepSingleByChannel(H,{GX,HouseWestField::FenceY-200,GZ},{GX,HouseWestField::FenceY+200,GZ},FQuat::Identity,ECC_Pawn,Shape,FQ);
    // Rebuild the unchanged pure fence geometry in memory, then compare with the installed world's actual vertices.
    // This does not attach components, change terrain or consume any shared RNG (Fence uses its own stream).
    FHallMesh ExpectedFence,ExpectedDetail;HouseWestField::Fence(ExpectedFence,ExpectedDetail);int32 FenceVertices=0;double MaxFenceDelta=0.;
    bool ExactFence=Fence&&Fence->GetNumSections()==ExpectedFence.Order.Num();
    if(Fence)for(int32 I=0;I<ExpectedFence.Order.Num();++I)
    {
        const FHallSection& Expected=ExpectedFence.Sections[ExpectedFence.Order[I]];const FProcMeshSection* Actual=Fence->GetProcMeshSection(I);
        if(!Actual||Actual->ProcVertexBuffer.Num()!=Expected.V.Num()||Actual->ProcIndexBuffer.Num()!=Expected.I.Num()){ExactFence=false;continue;}
        for(int32 J=0;J<Expected.V.Num();++J){++FenceVertices;MaxFenceDelta=FMath::Max(MaxFenceDelta,FVector::Dist(Fence->GetComponentTransform().TransformPosition(Actual->ProcVertexBuffer[J].Position),Expected.V[J]));}
        for(int32 J=0;J<Expected.I.Num();++J)ExactFence&=Actual->ProcIndexBuffer[J]==uint32(Expected.I[J]);
    }
    ExactFence&=FenceVertices>0&&MaxFenceDelta<.001;
    const bool Field=Plateau==12&&Connector==3&&FenceBlocks==6&&GateClear&&ExactFence&&Fence&&BodyReady(Fence)
        &&!Fence->CastShadow&&!Fence->bAffectDynamicIndirectLighting&&HouseWestField::FenceY==4520&&HouseWestField::GateWest==-7320&&HouseWestField::GateEast==-6780;
    Note(FString::Printf(TEXT("CHECK %s originalFieldProbes=%d/12 connectorSupported=%d/3 originalFenceBlocked=%d/6 originalGateClear=%d exactInstalledFence=%d fenceVertices=%d maxFenceVertexDeltaCm=%.6f fenceY=%.1f gate=[%.1f,%.1f]"),Field?TEXT("PASS"):TEXT("FAIL"),Plateau,Connector,FenceBlocks,int(GateClear),int(ExactFence),FenceVertices,MaxFenceDelta,HouseWestField::FenceY,HouseWestField::GateWest,HouseWestField::GateEast));
    return Geometry&&ISMComplete&&BadISM==0&&Parking&&LightBudget&&Trees&&Physical&&Dividers&&Field;
}

// Finds the actual floor under a planned walk point. Inside shells/stair envelopes the upper bound is the known floor,
// so roof tops are never accepted as shed/house floors. Everywhere else it must be today's terrain within3cm.
bool FloorAt(UWorld* W,AActor* Pawn,FVector2D P,float& Z)
{
    const float Terrain=HouseExteriorSite::GroundHeight(P.X,P.Y);float Top=Terrain+3,Minimum=Terrain-3;
    for(const auto& H:HouseFraternityRow::Houses())
    {
        if(FRect{H.L,H.R,H.F,H.B}.Contains(P)){Top=H.Floor+3;Minimum=H.Floor-3;break;}
        if(FRect{H.X()-200,H.X()+200,H.B,H.B+(H.Index<3?380.f:500.f)}.Contains(P)){Top=H.Floor+3;break;}
    }
    for(const FRect& R:Sheds)if(R.Contains(P)){Top=ShedFloor(R)+3;Minimum=ShedFloor(R)-3;break;}
    FHitResult H;if(!W->LineTraceSingleByChannel(H,FVector(P,Top+45),FVector(P,Minimum-30),ECC_Pawn,
        FCollisionQueryParams(SCENE_QUERY_STAT(FraternityRowWalkFloor),true,Pawn)))return false;
    Z=H.ImpactPoint.Z;return H.ImpactNormal.Z>.70f&&Z>=Minimum&&Z<=Top;
}
ACampusDoor* Door(UWorld* W,int32 Yard,const TCHAR* Leaf)
{
    const FName Id(*FString::Printf(TEXT("Campus_R%d_Rear%s"),Yard+1,Leaf));
    for(TActorIterator<ACampusDoor> It(W);It;++It)if(It->DoorId==Id)return *It;return nullptr;
}
void MakeRoute()
{
    S.Route.Reset();const auto& H=HouseFraternityRow::Houses()[S.Yard];const auto& P=Paths()[S.Yard];const FRect R=Sheds[S.Yard];
    const float C=(R.L+R.R)*.5f;auto Add=[&](FVector2D V,const TCHAR* Label,bool Shot=false){S.Route.Add({V,Label,Shot});};
    Add({H.X(),H.B+65},TEXT("cross rear doorway onto top landing"));
    for(int32 J=0;J<P.Num()-1;++J)Add(P[J],J==0?TEXT("rear steps bottom"):TEXT("own backyard route"));
    Add({C,R.F-240},TEXT("outbuilding front approach"));Add({C,R.F+35},TEXT("cross open outbuilding threshold"));
    Add({C,R.F+140},TEXT("inside own outbuilding"),true);Add({C,R.F-240},TEXT("leave outbuilding by real opening"));
    Add({P.Last().X,R.F-240},TEXT("pass beside outbuilding"));Add(P.Last(),TEXT("own fence approach Y4400"),true);
    for(int32 J=P.Num()-2;J>=0;--J)Add(P[J],TEXT("return along own yard path"));
    Add({H.X(),H.B+65},TEXT("climb rear steps onto landing"));Add({H.X(),H.B-170},TEXT("return through rear doorway inside house"),true);
    S.Leg=0;
}
bool OpenRear(AStoryFirstPersonCharacter* Pawn)
{
    if(!S.RearA.IsValid()||!S.RearB.IsValid()||S.RearA->bLocked||S.RearB->bLocked)return false;
    if(S.RearA->IsOpen()&&S.RearB->IsOpen())return true;
    if(S.RearA->IsOpen()!=S.RearB->IsOpen())return false;
    const bool Opened=S.RearA->Interact(Pawn);S.DoorOpens+=Opened;
    Note(FString::Printf(TEXT("DOOR house=R%d id=%s Interact=%d distance=%.2f openA=%d openB=%d"),S.Yard+1,*S.RearA->DoorId.ToString(),int(Opened),
        FVector::Dist(Pawn->GetActorLocation(),S.RearA->DoorwayCenter),int(S.RearA->IsOpen()),int(S.RearB->IsOpen())));
    return Opened;
}
void Tick(UWorld* W,ELevelTick,float)
{
    static const bool SceneFlag=FParse::Param(FCommandLine::Get(),TEXT("FraternityRowReview")),WalkFlag=FParse::Param(FCommandLine::Get(),TEXT("FraternityRowYardsWalkReview"));
    if(!W||!W->IsGameWorld()||(!SceneFlag&&!WalkFlag)||UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House"))return;
    const double Now=W->GetTimeSeconds();if(S.World.Get()!=W)
    {S=FState();S.World=W;S.Started=S.Step=Now;S.StartedUTC=ReviewLoadedUTC;S.Dir=FPaths::ProjectSavedDir()/(WalkFlag?TEXT("FraternityRowYardsWalkReview"):TEXT("FraternityRowReview"));IFileManager::Get().MakeDirectory(*S.Dir,true);}
    if(S.Phase<0)return;
    if(Now-S.Started>600){Finish(false,TEXT("review exceeded600second absolute budget"));return;}
    APlayerController* PC=W->GetFirstPlayerController();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;
    if(!Pawn||!Pawn->HasAuthority()||!Pawn->IsLocallyControlled()||!PC->IsLocalController())
    {if(Now-S.Started>20)Finish(false,TEXT("no local authority story pawn; run this review standalone"));return;}
    S.Walker=Pawn;
    if(S.Phase==0)
    {
        if(Now-S.Started<10||!HouseCalderTerrain::CollisionReady(W))return;
        if(!Scene(W,Pawn)){Finish(false,TEXT("actual component/collision/ground/light/tree/parking scene checks failed"));return;}
        if(!WalkFlag){Finish(true,TEXT("scene-only acceptance; no physical walk claim"));return;}
        if(!HouseFraternityRow::Enabled()||HouseFraternityRow::Houses().Num()!=5){Finish(false,TEXT("five-house upgrade required for yard walk"));return;}
        S.Phase=1;
    }
    UCampusInteriorsSubsystem* Sub=W->GetSubsystem<UCampusInteriorsSubsystem>();auto* Interiors=Sub?Sub->GetInteriors():nullptr;
    const auto& Houses=HouseFraternityRow::Houses();if(!Houses.IsValidIndex(S.Yard)||!Interiors){Finish(false,TEXT("missing row descriptor or campus interiors actor"));return;}
    const auto& House=Houses[S.Yard];const int32 Seed=HouseCampusInteriors::SeedFor(House.Id);
    if(S.Phase==1)
    {
        if(Seed<0){Finish(false,TEXT("row interior ID is not registered"));return;}
        if(!Interiors->Loaded().Contains(Seed))Interiors->BuildNow(Seed);
        S.Step=Now;S.Phase=2;return;
    }
    if(S.Phase==2)
    {
        AHouseCampusInteriors::FStats Stats;
        if(Now-S.Step>20){Finish(false,TEXT("row interior collision/static conversion not ready in20seconds"));return;}
        if(Now-S.Step<3||Interiors->PendingConversions(Seed)>0||!Interiors->Stats(Seed,Stats)||Stats.SolidParts==0||Stats.CollidingParts!=Stats.SolidParts)return;
        const FVector2D Start(House.X(),House.B-170);float Z=0;
        if(!FloorAt(W,Pawn,Start,Z)){Finish(false,TEXT("no own rear-interior starting floor"));return;}
        const float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const FVector Spawn(Start,Z+Half+3);FCollisionQueryParams Q(SCENE_QUERY_STAT(FraternityRowSetup),true,Pawn);
        if(W->OverlapBlockingTestByChannel(Spawn,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q))
        {Finish(false,TEXT("rear-interior setup capsule overlaps solid geometry"));return;}
        // Exactly one starting placement per yard. All subsequent door/step/shed/fence/return legs use real input.
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->SetActorLocation(Spawn,false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);PC->SetViewTarget(Pawn);PC->SetControlRotation(FRotator(0,90,0));++S.Setups;
        S.RearA=Door(W,S.Yard,TEXT("A"));S.RearB=Door(W,S.Yard,TEXT("B"));
        if(!OpenRear(Pawn)){Finish(false,TEXT("rear door pair missing/locked/inconsistent or normal Interact rejected"));return;}
        MakeRoute();S.Step=Now;S.Phase=3;Note(FString::Printf(TEXT("YARD R%d singleSetup=%d goals=%d builtSolid=%d cookedSolid=%d"),S.Yard+1,S.Setups,S.Route.Num(),Stats.SolidParts,Stats.CollidingParts));return;
    }
    if(S.Phase==3)
    {
        if(Now-S.Step<1.5)return;
        if(!S.RearA.IsValid()||!S.RearB.IsValid()||!S.RearA->IsOpen()||!S.RearB->IsOpen()){Finish(false,TEXT("rear leaves did not remain normally open"));return;}
        S.Phase=4;
    }
    if(S.Phase==4)
    {
        if(!S.Route.IsValidIndex(S.Leg))
        {++S.CompletedYards;Note(FString::Printf(TEXT("YARD PASS R%d walkedStepsShedFenceAndReturn=1"),S.Yard+1));
            if(S.Yard<4){++S.Yard;S.Phase=1;return;}S.Phase=6;S.FinishAt=Now+2;return;}
        float Z=0;if(!FloorAt(W,Pawn,S.Route[S.Leg].P,Z)){Finish(false,TEXT("no supported goal floor: ")+S.Route[S.Leg].Label);return;}
        S.Target=FVector(S.Route[S.Leg].P,Z);S.Best=MAX_flt;S.Step=S.Progress=Now;S.Phase=5;return;
    }
    if(S.Phase==5)
    {
        const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        PC->SetControlRotation(FRotator(-5,(S.Target-Feet).Rotation().Yaw,0));
        const float Distance=FVector::Dist2D(Feet,S.Target);if(Distance<S.Best-4){S.Best=Distance;S.Progress=Now;}
        if(Now-S.Progress>7||Now-S.Step>30){Finish(false,FString::Printf(TEXT("blocked/stalled R%d goal=%d %s feet=%s target=%s distance=%.2f"),S.Yard+1,S.Leg,*S.Route[S.Leg].Label,*Feet.ToString(),*S.Target.ToString(),Distance));return;}
        if(Now-S.LastFloor>.20)
        {
            S.LastFloor=Now;++S.FloorChecks;float Floor=0;const bool Supported=FloorAt(W,Pawn,FVector2D(Feet),Floor);
            S.FeetCSV+=FString::Printf(TEXT("%.3f,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f\n"),Now,S.Yard+1,S.Leg,Feet.X,Feet.Y,Feet.Z,Floor,Feet.Z-Floor);
            if(!Supported||Feet.Z<Floor-8||Feet.Z-Floor>45){Finish(false,TEXT("missing floor or ground drop during real walk at ")+Feet.ToString());return;}
        }
        if(S.RearA.IsValid()&&FVector::Dist(Pawn->GetActorLocation(),S.RearA->DoorwayCenter)<250&&(!S.RearA->IsOpen()||!S.RearB->IsOpen()))
        {if(!OpenRear(Pawn)){Finish(false,TEXT("normal rear-door interaction failed on return"));return;}S.Step=Now;S.Phase=3;return;}
        if(Distance>32||FMath::Abs(Feet.Z-S.Target.Z)>8)return;
        Pawn->GetCharacterMovement()->StopMovementImmediately();++S.Reached;
        Note(FString::Printf(TEXT("WALK R%d leg=%d/%d %s feet=%s realMovementInput=1"),S.Yard+1,S.Leg+1,S.Route.Num(),*S.Route[S.Leg].Label,*Feet.ToString()));
        if(S.Route[S.Leg].bShot){const FString File=S.Dir/FString::Printf(TEXT("R%d_%02d.png"),S.Yard+1,S.Leg+1);FScreenshotRequest::RequestScreenshot(File,false,false);S.Captures.Add(File);}
        ++S.Leg;S.Phase=4;return;
    }
    if(S.Phase==6&&Now>=S.FinishAt&&!FScreenshotRequest::IsScreenshotRequested())
    {
        bool Shots=S.Captures.Num()==15;for(const FString& File:S.Captures)Shots&=IFileManager::Get().FileSize(*File)>0;
        Finish(S.CompletedYards==5&&S.Setups==5&&Shots,TEXT("all5 continuous rear-door/steps/yard/shed/fence/return routes;15 saved captures; standalone check"));
    }
}
struct FRegistration
{
    FRegistration()
    {
        FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);
        FCoreDelegates::OnBeginFrame.AddLambda([]
        {if(S.Phase==5&&S.Walker.IsValid())S.Walker->AddMovementInput((S.Target-S.Walker->GetActorLocation()).GetSafeNormal2D(),1.f);});
    }
} Registration;
}
#endif
