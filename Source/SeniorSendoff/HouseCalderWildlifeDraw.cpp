#include "HouseCalderWildlifePrivate.h"
#include "HouseBlackHallMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"
#include "HouseCalderTerrain.h"

namespace HouseCalderWildlifeDrawImpl
{
const FName Brown(TEXT("CalderWildlifeBrown")),White(TEXT("CalderWildlifeWhite")),Horn(TEXT("CalderWildlifeHorn")),Contact(TEXT("CalderWildlifeContact"));
FVector4f Colour(FName Key)
{return Key==White?FVector4f(.68,.65,.57,1):Key==Horn?FVector4f(.27,.20,.12,1):FVector4f(.18,.155,.12,1);}
void Describe(const FHallMesh& From,FMeshDescription& Description)
{
    FStaticMeshAttributes A(Description);A.Register();auto Positions=A.GetVertexPositions();auto Normals=A.GetVertexInstanceNormals();
    auto Tangents=A.GetVertexInstanceTangents();auto Signs=A.GetVertexInstanceBinormalSigns();auto UVs=A.GetVertexInstanceUVs();auto Colours=A.GetVertexInstanceColors();
    UVs.SetNumChannels(1);auto Names=A.GetPolygonGroupMaterialSlotNames();const auto Group=Description.CreatePolygonGroup();Names[Group]=TEXT("CalderWildlifePart");
    for(const FName Key:From.Order)
    {
        const auto& Part=From.Sections[Key];TArray<FVertexID> V;
        for(const auto& P:Part.V){const auto Id=Description.CreateVertex();Positions[Id]=FVector3f(P);V.Add(Id);}
        for(int32 I=0;I<Part.I.Num();I+=3)
        {
            TArray<FVertexInstanceID,TInlineAllocator<3>> Triangle;
            for(int32 J=0;J<3;++J)
            {
                const int32 Index=Part.I[I+J];const auto W=Description.CreateVertexInstance(V[Index]);
                Normals[W]=FVector3f(Part.N[Index]);Tangents[W]=FVector3f(Part.T[Index].TangentX);Signs[W]=Part.T[Index].bFlipTangentY?-1.f:1.f;
                UVs.Set(W,0,FVector2f(Part.UV[Index]));Colours[W]=Colour(Key);Triangle.Add(W);
            }
            Description.CreateTriangle(Group,Triangle);
        }
    }
}
void Tail(FHallMesh& M,int32 Lod)
{
    const int32 Rings=Lod==0?10:Lod==1?7:4,Sides=Lod==0?12:Lod==1?8:6;
    for(int32 R=0;R<Rings;++R)for(int32 Side=0;Side<Sides;++Side)
    {
        auto P=[&](int32 Ring,int32 S)
        {const double T=double(Ring)/Rings,Angle=double(S)/Sides*2*PI,Width=FMath::Pow(FMath::Max(0.,FMath::Sin(PI*T)),.72);return FVector(-27*T,FMath::Cos(Angle)*Width*6.2,FMath::Sin(Angle)*Width*2.7-T*4.);};
        const FVector A=P(R,Side),B=P(R,Side+1),C=P(R+1,Side+1),D=P(R+1,Side);
        const FVector N=-(FVector::CrossProduct(B-A,C-A)+FVector::CrossProduct(C-A,D-A)).GetSafeNormal();
        M.Quad(N.Z<0?White:Brown,A,B,C,D,N,FHallUV::World(8));
    }
}
void Taper(FHallMesh& M,const TArray<FVector>& Points,float Radius,int32 Sides)
{
    for(int32 I=1;I<Points.Num();++I)
    {
        const FVector Axis=(Points[I]-Points[I-1]).GetSafeNormal();const FVector U=FVector::CrossProduct(Axis,FMath::Abs(Axis.Z)>.95?FVector::RightVector:FVector::UpVector).GetSafeNormal();const FVector V=FVector::CrossProduct(Axis,U);
        const double A=FMath::Lerp(double(Radius),.12,double(I-1)/(Points.Num()-1)),B=FMath::Lerp(double(Radius),.12,double(I)/(Points.Num()-1));
        for(int32 S=0;S<Sides;++S)
        {
            const FVector N0=U*FMath::Cos(S*2.*PI/Sides)+V*FMath::Sin(S*2.*PI/Sides),N1=U*FMath::Cos((S+1)*2.*PI/Sides)+V*FMath::Sin((S+1)*2.*PI/Sides);
            M.Quad(Horn,Points[I-1]+N0*A,Points[I-1]+N1*A,Points[I]+N1*B,Points[I]+N0*B,(N0+N1).GetSafeNormal(),FHallUV::World(6));
        }
    }
}
void Antlers(FHallMesh& M,int32 Lod)
{
    const int32 Sides=Lod==0?12:Lod==1?8:5;
    for(const float Sign:{-1.f,1.f})
    {
        auto V=[&](double X,double Y,double Z){return FVector(X,Y*Sign,Z);};
        Taper(M,{V(0,6,0),V(-5,11,8),V(-6,18,17),V(0,24,25),V(10,22,31),V(20,12,34)},2.1f,Sides);
        Taper(M,{V(-4,10,7),V(2,9,16),V(8,8,20)},1.1f,Sides);
        Taper(M,{V(-5,18,18),V(-6,21,31),V(-3,22,43)},1.3f,Sides);
        Taper(M,{V(5,23,28),V(4,27,38),V(8,28,46)},1.0f,Sides);
    }
}
UStaticMesh* Mesh(UObject* Outer,int32 Kind,UMaterialInterface* Material)
{
    auto* Result=NewObject<UStaticMesh>(Outer,NAME_None,RF_Transient);Result->bAllowCPUAccess=true;
    const FName Slot(TEXT("CalderWildlifePart"));Result->GetStaticMaterials().Add(FStaticMaterial(Material,Slot
#if WITH_EDITOR
        ,Slot
#endif
    ));
    TArray<FMeshDescription> Descriptions;Descriptions.SetNum(3);TArray<const FMeshDescription*> Sources;
    for(int32 Lod=0;Lod<3;++Lod)
    {
        FHallMesh Part;
        if(Kind==1)Tail(Part,Lod);else if(Kind==2)Antlers(Part,Lod);
        else Part.QuadUV(Contact,{-100,-40,0},{100,-40,0},{100,40,0},{-100,40,0},FVector::UpVector,{0,0},{1,0},{1,1},{0,1},FVector::ForwardVector);
        Describe(Part,Descriptions[Lod]);Sources.Add(&Descriptions[Lod]);
    }
    UStaticMesh::FBuildMeshDescriptionsParams Params;Params.bFastBuild=true;Params.bBuildSimpleCollision=false;Params.bAllowCpuAccess=true;
    Result->BuildFromMeshDescriptions(Sources,Params);
    auto* Data=Result->GetRenderData();if(!Data || Data->LODResources.Num()!=3)return nullptr;
    const float Screens[]={1.f,.3f,.08f};for(int32 I=0;I<3;++I)Data->ScreenSize[I].Default=Screens[I];
    return Result;
}
void Pack(const TArray<FTransform>& Controls,float* Into)
{
    for(int32 I=0;I<Controls.Num();++I)
    {
        const auto& T=Controls[I];FQuat Q=T.GetRotation().GetNormalized();if(Q.W<0.)Q=Q*-1.;const FVector P=T.GetTranslation();
        Into[I*6]=float(Q.X);Into[I*6+1]=float(Q.Y);Into[I*6+2]=float(Q.Z);
        Into[I*6+3]=float(P.X);Into[I*6+4]=float(P.Y);Into[I*6+5]=float(P.Z);
    }
}
void MediumSoles(const FHouseCalderWildlifeState& S,FHouseCalderWildlifeAnimal& A,double Now,double KeyStart,TArray<FTransform>& Pose)
{
    // Keep the existing15Hz body/limb keys. A world-space foot interpolation
    // must stop at lift-off and touchdown; blending across either boundary
    // otherwise moves a planted hoof, especially for the shorter fawn stance.
    const bool Moving=HouseCalderWildlifePose::Speed(S.Mood)>0.;
    const double Age=FMath::Max(0.,Now-S.StartTime);
    if(A.MediumStateStart!=S.StartTime || A.MediumStateMood!=uint8(S.Mood))
    {A.MediumStateStart=S.StartTime;A.MediumStateMood=uint8(S.Mood);for(int32 Leg=0;Leg<4;++Leg)A.MediumCycles[Leg]=MIN_int64;}
    for(int32 Leg=0;Leg<4;++Leg)
    {
        const auto Foot=HouseCalderWildlifePose::FootPhase(S,A,Leg,Now);
        const int64 Cycle=Foot.Cycle;const double Touch=Foot.Touch;
        if(A.MediumCycles[Leg]!=Cycle)
        {
            if(Moving && A.MediumCycles[Leg]!=MIN_int64 && A.MediumCycles[Leg]+1==Cycle)A.MediumStance[Leg]=A.MediumNextStance[Leg];
            else A.MediumStance[Leg]=HouseCalderWildlifePose::StanceSole(S,A,Leg,Touch);
            A.MediumNextStance[Leg]=Moving?HouseCalderWildlifePose::StanceSole(S,A,Leg,Foot.NextTouch):A.MediumStance[Leg];A.MediumCycles[Leg]=Cycle;
        }
        const double StampLift=S.Mood==ECalderWildlifeMood::Alarm && Leg==0?FMath::Max(0.,FMath::Sin(FMath::Fmod(Age,3.2)*PI/.65))*11.*A.Scale:0.;
        A.Planted[Leg]=(!Moving || Foot.Planted) && StampLift<.1;A.CurrentFootCycles[Leg]=Cycle;const int32 Joint=7+Leg*4;
        if(!Moving || Foot.Planted)
        {Pose[Joint]=A.MediumStance[Leg];if(StampLift>0.)Pose[Joint].AddToTranslation(FVector(0,0,StampLift));}
        else
        {
            const double LiftOff=Foot.Lift,NextTouch=Foot.NextTouch;
            const double FromTime=FMath::Max(KeyStart,LiftOff),ToTime=FMath::Min(KeyStart+1./15.,NextTouch);
            const FTransform& From=KeyStart<LiftOff?A.MediumStance[Leg]:A.KeyControls[0][Joint];
            const FTransform& To=KeyStart+1./15.>NextTouch?A.MediumNextStance[Leg]:A.KeyControls[1][Joint];
            const float Blend=float(FMath::Clamp((Now-FromTime)/FMath::Max(1.e-9,ToTime-FromTime),0.,1.));Pose[Joint].Blend(From,To,Blend);
        }
    }
}
}
bool AHouseCalderWildlifeDirector::CreateVisuals()
{
    const double Start=FPlatformTime::Seconds();
    UStaticMesh* Body=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Wildlife/SM_CalderDeerBody.SM_CalderDeerBody"));
    auto* PartMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Wildlife/M_CalderWildlifePart.M_CalderWildlifePart"));
    auto* ContactMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Wildlife/M_CalderWildlifeContact.M_CalderWildlifeContact"));
    if(!Body || !PartMaterial || !ContactMaterial || !Body->bAllowCPUAccess || !Body->GetRenderData())return false;
    const auto& Data=Body->GetRenderData()->LODResources[0];if(Data.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords()<5)return false;
    int32 RawSoleVertices[4]={};
    for(uint32 V=0;V<Data.GetNumVertices();++V)
    {
        const FVector P(Data.VertexBuffers.PositionVertexBuffer.VertexPosition(V));
        if(P.Z>1.2)continue;
        for(int32 K=0;K<4;++K)
        {
            const auto UV=Data.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(V,K+1);const int32 Joint=FMath::RoundToInt(UV.X);
            if(UV.Y>.999f && Joint>=7 && Joint<=19 && (Joint-7)%4==0)
            {const int32 Leg=(Joint-7)/4;++RawSoleVertices[Leg];Runtime->Soles[Leg].AddUnique(P);}
        }
    }
    for(int32 Leg=0;Leg<4;++Leg)if(Runtime->Soles[Leg].IsEmpty())return false;
    UStaticMesh* Meshes[]={Body,HouseCalderWildlifeDrawImpl::Mesh(this,1,PartMaterial),HouseCalderWildlifeDrawImpl::Mesh(this,2,PartMaterial),HouseCalderWildlifeDrawImpl::Mesh(this,3,ContactMaterial)};
    const TCHAR* Names[]={TEXT("Wildlife_DeerBody"),TEXT("Wildlife_DeerTail"),TEXT("Wildlife_DeerAntlers"),TEXT("Wildlife_Contact")};
    for(int32 Kind=0;Kind<4;++Kind)
    {
        if(!Meshes[Kind])return false;OwnedMeshes.Add(Meshes[Kind]);
        auto* Part=NewObject<UInstancedStaticMeshComponent>(this,FName(Names[Kind]));AddInstanceComponent(Part);Part->SetupAttachment(RootComponent);
        Part->SetMobility(EComponentMobility::Movable);Part->SetStaticMesh(Meshes[Kind]);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetGenerateOverlapEvents(false);
        Part->SetCanEverAffectNavigation(false);Part->SetCastShadow(false);Part->bAffectDistanceFieldLighting=false;Part->bAffectDynamicIndirectLighting=false;
        Part->SetHasPerInstancePrevTransforms(true);Part->SetCullDistances(15000,30000);if(Kind==0)Part->SetNumCustomDataFloats(HouseCalderWildlifePose::CustomFloats);
        const int32 Count=Kind==2?1:States.Items.Num();Runtime->Current[Kind].Init(FTransform::Identity,Count);Runtime->Previous[Kind]=Runtime->Current[Kind];
        Part->AddInstances(Runtime->Current[Kind],false,false,false);Part->SetVisibility(false);Part->RegisterComponent();DrawParts.Add(Part);
    }
    Runtime->Custom.SetNumZeroed(States.Items.Num()*HouseCalderWildlifePose::CustomFloats);
    UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_DRAW species=deer components=4 animals=10 lods=3 controls=21 customFloats=253 uniqueSolePositions=%d/%d/%d/%d rawSoleVertices=%d/%d/%d/%d runtimeSkeletons=0 actorsPerAnimal=0 collision=0 shadows=0 buildMs=%.2f"),Runtime->Soles[0].Num(),Runtime->Soles[1].Num(),Runtime->Soles[2].Num(),Runtime->Soles[3].Num(),RawSoleVertices[0],RawSoleVertices[1],RawSoleVertices[2],RawSoleVertices[3],(FPlatformTime::Seconds()-Start)*1000.);
    return true;
}
void AHouseCalderWildlifeDirector::Draw(double Now,float)
{
    struct FView{FVector Eye,Forward;double CosAngle;};TArray<FView> Views;
    for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
        if(auto* PC=It->Get())if(PC->IsLocalController()&&PC->PlayerCameraManager)
            Views.Add({PC->PlayerCameraManager->GetCameraLocation(),PC->PlayerCameraManager->GetCameraRotation().Vector(),FMath::Cos(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle()*.5+10.))});
    const double SceneNow=HouseCalderWildlife::SceneTime(GetWorld());
    auto SceneAt=[Now,SceneNow](double At){return HouseCalderWildlife::ScenePinned()?SceneNow:FMath::Fmod(FMath::Fmod(SceneNow+At-Now,1800.)+1800.,1800.);};
    bool NearAny=false,Changed=false;
    for(int32 I=0;I<States.Items.Num();++I)
    {
        const auto& S=States.Items[I];auto& A=Runtime->Animals[I];const FVector Position=AnimalPosition(I,Now);
        double Distance=TNumericLimits<double>::Max();bool InView=false;
        for(const auto& View:Views)
        {
            const FVector Delta=Position+FVector(0,0,80*A.Scale)-View.Eye;Distance=FMath::Min(Distance,Delta.Size());
            InView|=Delta.SizeSquared()<10000. || FVector::DotProduct(Delta.GetSafeNormal(),View.Forward)>View.CosAngle;
        }
        NearAny|=Distance<30000.;
        const bool First=A.Controls.IsEmpty(),Full=First||(InView&&Distance<4000.),Medium=InView&&Distance<15000.;
        const bool Moving=HouseCalderWildlifePose::Speed(S.Mood)>0.;
        const bool Stopping=A.WasMoving&&!Moving;
        if(!First&&!Full&&!Medium&&!Stopping&&(!Moving||Distance>30000.||Now-A.LastRoot<.1))continue;
        A.LastPoseTier=Full?2:Medium?1:0;
        const TArray<FTransform> PreviousControls=A.Controls;const FTransform OldRoot=Runtime->Current[0][I];
        FTransform RootPose,TailPose,AntlerPose,ContactPose;TArray<FTransform> Pose;
        if(Full)
            HouseCalderWildlifePose::Evaluate(S,A,Now,SceneNow,RootPose,TailPose,AntlerPose,ContactPose,Pose);
        else if(Medium)
        {
            const double KeyStart=S.StartTime+FMath::FloorToDouble((Now-S.StartTime)*15.)/15.;
            const double Scene0=SceneAt(KeyStart),Scene1=SceneAt(KeyStart+1./15.);
            const bool SameState=A.KeyStateValid&&A.KeyScale==A.Scale&&HouseCalderWildlifePose::SameInputs(S,A.KeyState);
            if(A.KeyStart!=KeyStart||!SameState)
            {
                // Exact time identity also preserves discontinuous stamp/plant
                // boundaries; nearby floating-point keys are recomputed.
                const bool Reuse=SameState&&A.KeyControls[1].Num()==HouseCalderWildlifePose::Controls&&A.KeyStart+1./15.==KeyStart&&A.KeyScene[1]==Scene0;
                if(Reuse)
                {
                    A.KeyRoot[0]=A.KeyRoot[1];A.KeyTail[0]=A.KeyTail[1];A.KeyAntler[0]=A.KeyAntler[1];A.KeyContact[0]=A.KeyContact[1];A.KeyControls[0]=A.KeyControls[1];
                    A.LocalKeyControls[0]=A.LocalKeyControls[1];A.LocalKeyTail[0]=A.LocalKeyTail[1];A.LocalKeyAntler[0]=A.LocalKeyAntler[1];A.LocalKeyContact[0]=A.LocalKeyContact[1];++A.ReusedKeys;
                    if(HouseCalderWildlifePose::CacheReview())
                    {
                        FTransform FreshRoot,FreshTail,FreshAntler,FreshContact;TArray<FTransform> Fresh;
                        HouseCalderWildlifePose::Evaluate(S,A,KeyStart,Scene0,FreshRoot,FreshTail,FreshAntler,FreshContact,Fresh);
                        HouseCalderWildlifePose::CheckCache(A,A.KeyRoot[0],FreshRoot);HouseCalderWildlifePose::CheckCache(A,A.KeyTail[0],FreshTail);HouseCalderWildlifePose::CheckCache(A,A.KeyAntler[0],FreshAntler);HouseCalderWildlifePose::CheckCache(A,A.KeyContact[0],FreshContact);
                        for(int32 Joint=0;Joint<Fresh.Num();++Joint)HouseCalderWildlifePose::CheckCache(A,A.KeyControls[0][Joint],Fresh[Joint]);
                    }
                }
                else HouseCalderWildlifePose::Evaluate(S,A,KeyStart,Scene0,A.KeyRoot[0],A.KeyTail[0],A.KeyAntler[0],A.KeyContact[0],A.KeyControls[0]);
                A.KeyStart=KeyStart;
                HouseCalderWildlifePose::Evaluate(S,A,KeyStart+1./15.,Scene1,A.KeyRoot[1],A.KeyTail[1],A.KeyAntler[1],A.KeyContact[1],A.KeyControls[1]);
                for(int32 Key=Reuse?1:0;Key<2;++Key)
                {
                    A.LocalKeyControls[Key].SetNum(HouseCalderWildlifePose::Controls);
                    for(int32 Joint=0;Joint<HouseCalderWildlifePose::Controls;++Joint)
                        if(!(Joint>=7&&Joint<=19&&(Joint-7)%4==0))A.LocalKeyControls[Key][Joint]=A.KeyControls[Key][Joint].GetRelativeTransform(A.KeyRoot[Key]);
                    A.LocalKeyTail[Key]=A.KeyTail[Key].GetRelativeTransform(A.KeyRoot[Key]);A.LocalKeyAntler[Key]=A.KeyAntler[Key].GetRelativeTransform(A.KeyRoot[Key]);A.LocalKeyContact[Key]=A.KeyContact[Key].GetRelativeTransform(A.KeyRoot[Key]);
                }
                A.KeyScene[0]=Scene0;A.KeyScene[1]=Scene1;A.KeyState=S;A.KeyStateValid=true;A.KeyScale=A.Scale;
            }
            const float Alpha=float(FMath::Clamp((Now-KeyStart)*15.,0.,1.));Pose.SetNum(HouseCalderWildlifePose::Controls);
            FVector Direction;const FVector At=HouseCalderWildlifePose::Root(S,A,Now,&Direction);RootPose=HouseCalderWildlifePose::Frame(S,A,Now);
            for(int32 Joint=0;Joint<Pose.Num();++Joint)
            {
                if(Joint>=7&&Joint<=19&&(Joint-7)%4==0)Pose[Joint].Blend(A.KeyControls[0][Joint],A.KeyControls[1][Joint],Alpha);
                else
                {
                    FTransform Local;Local.Blend(A.LocalKeyControls[0][Joint],A.LocalKeyControls[1][Joint],Alpha);Pose[Joint]=Local*RootPose;
                }
            }
            HouseCalderWildlifeDrawImpl::MediumSoles(S,A,Now,KeyStart,Pose);
            FTransform LocalTail,LocalAntler,LocalContact;
            LocalTail.Blend(A.LocalKeyTail[0],A.LocalKeyTail[1],Alpha);TailPose=LocalTail*RootPose;
            LocalAntler.Blend(A.LocalKeyAntler[0],A.LocalKeyAntler[1],Alpha);AntlerPose=LocalAntler*RootPose;
            LocalContact.Blend(A.LocalKeyContact[0],A.LocalKeyContact[1],Alpha);ContactPose=LocalContact*RootPose;
        }
        else
        {
            FVector Direction;const FVector At=HouseCalderWildlifePose::Root(S,A,Now,&Direction);RootPose=HouseCalderWildlifePose::Frame(S,A,Now);
            for(const FTransform& T:A.Controls)Pose.Add(T.GetRelativeTransform(OldRoot)*RootPose);
            TailPose=Runtime->Current[1][I].GetRelativeTransform(OldRoot)*RootPose;
            AntlerPose=S.Variant==1?Runtime->Current[2][0].GetRelativeTransform(OldRoot)*RootPose:FTransform::Identity;
            ContactPose=Runtime->Current[3][I].GetRelativeTransform(OldRoot)*RootPose;
        }
        const bool Stop=A.WasMoving&&!Moving;A.WasMoving=Moving;A.Controls=MoveTemp(Pose);A.LastRoot=Now;
        const bool Reset=First||Stop||!Runtime->Visible;
        Runtime->Previous[0][I]=Reset?RootPose:Runtime->Current[0][I];Runtime->Current[0][I]=RootPose;
        Runtime->Previous[1][I]=Reset?TailPose:Runtime->Current[1][I];Runtime->Current[1][I]=TailPose;
        Runtime->Previous[3][I]=Reset?ContactPose:Runtime->Current[3][I];Runtime->Current[3][I]=ContactPose;
        if(S.Variant==1){Runtime->Previous[2][0]=Reset?AntlerPose:Runtime->Current[2][0];Runtime->Current[2][0]=AntlerPose;}
        float* Custom=Runtime->Custom.GetData()+I*HouseCalderWildlifePose::CustomFloats;
        HouseCalderWildlifeDrawImpl::Pack(A.Controls,Custom);
        HouseCalderWildlifeDrawImpl::Pack(Reset?A.Controls:PreviousControls,Custom+HouseCalderWildlifePose::CurrentFloats);
        Custom[HouseCalderWildlifePose::CustomFloats-1]=A.Scale;
        DrawParts[0]->SetCustomData(I,MakeArrayView(Custom,HouseCalderWildlifePose::CustomFloats),false);
        // Moving planted soles are checked every full-pose frame. Idle soles
        // are sampled at10Hz; repeating identical standing geometry each render
        // frame adds no contact information and needlessly taxes the tick.
        if(Full && (Moving || Now>=A.NextIdleSoleSample))
        {
            A.NextIdleSoleSample=Now+.1;auto& MoodSamples=Runtime->SoleWindows.FindOrAdd(uint8(S.Mood));
            for(int32 Leg=0;Leg<4;++Leg)
            {
                if(!A.Planted[Leg]){A.FootCycles[Leg]=MIN_int64;continue;}
                FVector Centre=FVector::ZeroVector;double Lowest=TNumericLimits<double>::Max();
                for(const FVector& V:Runtime->Soles[Leg])
                {const FVector P=A.Controls[7+Leg*4].TransformPosition(V);Centre+=P;Lowest=FMath::Min(Lowest,P.Z-HouseCalderTerrain::SurfaceZ(P.X,P.Y));}
                Centre/=Runtime->Soles[Leg].Num();HouseCalderWildlifeStats::Sample(Runtime->HoofGaps,Runtime->GapCursor,FMath::Abs(Lowest));
                HouseCalderWildlifeStats::Sample(MoodSamples.Gaps,MoodSamples.GapCursor,FMath::Abs(Lowest));
                if(A.FootCycles[Leg]==A.CurrentFootCycles[Leg])
                {
                    const double Drift=FVector::Distance(Centre,A.Anchors[Leg]);HouseCalderWildlifeStats::Sample(Runtime->HoofDrifts,Runtime->DriftCursor,Drift);
                    HouseCalderWildlifeStats::Sample(MoodSamples.Drifts,MoodSamples.DriftCursor,Drift);
                }
                else{A.FootCycles[Leg]=A.CurrentFootCycles[Leg];A.Anchors[Leg]=Centre;}
                A.Hooves[Leg]=Centre;
            }
        }
        Changed=true;
    }
    if(NearAny)Runtime->HideAfter=Now+5.;
    const bool Visible=NearAny || (Runtime->Visible&&Now<Runtime->HideAfter);
    if(Visible!=Runtime->Visible){for(UInstancedStaticMeshComponent* Part:DrawParts)Part->SetVisibility(Visible);Runtime->Visible=Visible;}
    if(Changed)
        for(int32 Kind=0;Kind<4;++Kind)
            DrawParts[Kind]->BatchUpdateInstancesTransforms(0,Runtime->Current[Kind],Runtime->Previous[Kind],false,false,false);
    if(Changed && Now>=Runtime->NextBounds)
    {
        Runtime->NextBounds=Now+.5;
        for(UInstancedStaticMeshComponent* Part:DrawParts){Part->UpdateBounds();Part->MarkRenderTransformDirty();}
    }
}
