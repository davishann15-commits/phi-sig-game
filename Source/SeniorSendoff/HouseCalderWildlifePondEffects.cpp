// Opt-in fixed pools for contact,V wakes,splash drops and splash rings.
// Same director/shared analytic clock;no collision,actors,lights or Niagara.
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifePrivate.h"
#include "HouseCalderWildlifePondWater.h"
#include "HouseBlackHallMesh.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderWater.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
namespace HouseCalderWildlifePondEffectsImpl
{
bool Enabled(){static const bool B=HouseCalderWildlife::BirdsEnabled()||FParse::Param(FCommandLine::Get(),TEXT("CalderGooseEffectsTrial"))||FParse::Param(FCommandLine::Get(),TEXT("CalderGooseAudioTrial"));return B;}
constexpr int32 Counts[4]={16,32,16,16};
const FName WaterSlot(TEXT("CalderBirdWater"));
void Describe(const FHallMesh& From,FMeshDescription& D)
{
    FStaticMeshAttributes A(D);A.Register();auto P=A.GetVertexPositions();auto N=A.GetVertexInstanceNormals();auto T=A.GetVertexInstanceTangents();auto B=A.GetVertexInstanceBinormalSigns();auto UV=A.GetVertexInstanceUVs();auto C=A.GetVertexInstanceColors();UV.SetNumChannels(1);const auto G=D.CreatePolygonGroup();A.GetPolygonGroupMaterialSlotNames()[G]=WaterSlot;
    for(FName Key:From.Order)
    {
        const auto& S=From.Sections[Key];TArray<FVertexID> V;for(const FVector& At:S.V){auto Id=D.CreateVertex();P[Id]=FVector3f(At);V.Add(Id);}
        for(int32 I=0;I<S.I.Num();I+=3)
        {
            TArray<FVertexInstanceID,TInlineAllocator<3>> Triangle;
            for(int32 K=0;K<3;++K){const int32 J=S.I[I+K];const auto W=D.CreateVertexInstance(V[J]);N[W]=FVector3f(S.N[J]);T[W]=FVector3f(S.T[J].TangentX);B[W]=S.T[J].bFlipTangentY?-1.f:1.f;UV.Set(W,0,FVector2f(S.UV[J]));const float Alpha=float(FMath::Sin(PI*FMath::Clamp(S.UV[J].Y,0.,1.))*FMath::Pow(FMath::Max(0.,1.-S.UV[J].X),.7));C[W]=FVector4f(1,1,1,Alpha);Triangle.Add(W);}D.CreateTriangle(G,Triangle);
        }
    }
}
UStaticMesh* Mesh(UObject* Outer,int32 Kind,UMaterialInterface* Material)
{
    auto* Result=NewObject<UStaticMesh>(Outer,NAME_None,RF_Transient);Result->bAllowCPUAccess=true;
    Result->GetStaticMaterials().Add(FStaticMaterial(Material,WaterSlot
#if WITH_EDITOR
        ,WaterSlot
#endif
    ));
    TArray<FMeshDescription> Ds;Ds.SetNum(3);TArray<const FMeshDescription*> Sources;
    for(int32 Lod=0;Lod<3;++Lod)
    {
        FHallMesh M;
        if(Kind==1)
        {
            const int32 Segments=Lod==0?6:Lod==1?3:2;
            for(double Sign:{-1.,1.})for(int32 S=0;S<Segments;++S)for(int32 Half=0;Half<2;++Half)
            {
                const double A=double(S)/Segments,B=double(S+1)/Segments,Side0=Half==0?-1.:0.,Side1=Half==0?0.:1.;
                auto At=[Sign](double T,double Side){return FVector(-20.-70.*T,Sign*(8.+23.*T)+Side*(2.+2.*T),0.);};
                M.QuadUV(WaterSlot,At(A,Side0),At(A,Side1),At(B,Side1),At(B,Side0),FVector::UpVector,{A,(Side0+1.)*.5},{A,(Side1+1.)*.5},{B,(Side1+1.)*.5},{B,(Side0+1.)*.5},FVector::ForwardVector);
            }
        }
        else if(Kind==2)
        {
            const int32 Drops=Lod==0?8:Lod==1?6:4;
            for(int32 I=0;I<Drops;++I)
            {
                const double Angle=I*2.*PI/Drops;const FVector O(FMath::Cos(Angle)*14.,FMath::Sin(Angle)*14.,-(I%3)*1.7);
                for(int32 Plane=0;Plane<2;++Plane)
                {const FVector Side=Plane?FVector::RightVector:FVector::ForwardVector,N=Plane?-FVector::ForwardVector:FVector::RightVector;M.QuadUV(WaterSlot,O+FVector(0,0,4),O+Side*3.,O-FVector(0,0,3),O-Side*3.,N,{0,.5},{0,.5},{0,.5},{0,.5},Side);}
            }
        }
        else
        {
            const int32 Sides=Lod==0?40:Lod==1?24:12;
            for(int32 I=0;I<Sides;++I)for(int32 Half=0;Half<2;++Half)
            {
                const double A=I*2.*PI/Sides,B=(I+1)*2.*PI/Sides,R0=Half==0?44.:48.,R1=Half==0?48.:52.,V0=Half*.5,V1=(Half+1)*.5;
                auto At=[](double A,double R){return FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,0.);};
                M.QuadUV(WaterSlot,At(A,R0),At(A,R1),At(B,R1),At(B,R0),FVector::UpVector,{0,V0},{0,V1},{0,V1},{0,V0},FVector::ForwardVector);
            }
        }
        Describe(M,Ds[Lod]);Sources.Add(&Ds[Lod]);
    }
    UStaticMesh::FBuildMeshDescriptionsParams P;P.bFastBuild=true;P.bAllowCpuAccess=true;P.bBuildSimpleCollision=false;Result->BuildFromMeshDescriptions(Sources,P);if(!Result->GetRenderData())return nullptr;
    for(int32 I=0;I<3;++I)Result->GetRenderData()->ScreenSize[I].Default=I==0?1.f:I==1?.3f:.08f;Result->SetPositiveBoundsExtension(FVector(200));Result->SetNegativeBoundsExtension(FVector(200));Result->CalculateExtendedBounds();return Result;
}
}
bool HouseCalderWildlifePondWater::WetEnvelope(const FVector& P,double Radius)
{
    for(const FVector2D D:{FVector2D(0,0),FVector2D(Radius,0),FVector2D(-Radius,0),FVector2D(0,Radius),FVector2D(0,-Radius),FVector2D(Radius*.7071,Radius*.7071),FVector2D(-Radius*.7071,Radius*.7071),FVector2D(Radius*.7071,-Radius*.7071),FVector2D(-Radius*.7071,-Radius*.7071)})
    {float Water=0.f,Deck=0.f;if(!HouseCalderWater::WaterAt(FVector2D(P)+D,Water)||FMath::Abs(Water-P.Z)>1.||HouseCalderWater::DeckAt(FVector2D(P)+D,Deck))return false;}return true;
}
void AHouseCalderWildlifeDirector::DrawPondEffects(double Now)
{
    using namespace HouseCalderWildlifePondEffectsImpl;
    using HouseCalderWildlifePondWater::WetEnvelope;
    if(!Enabled()||!Runtime||!Runtime->Pond.Ready||Runtime->Pond.Failed||GetNetMode()==NM_DedicatedServer)return;auto& P=Runtime->Pond;
    if(!P.EffectsAttempted)
    {
        P.EffectsAttempted=true;auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Wildlife/M_CalderBirdWater.M_CalderBirdWater"));
        if(!Material||DrawParts.Num()<4){P.EffectsFailed=true;UE_LOG(LogTemp,Error,TEXT("CALDER_POND_EFFECT_FAILED missing owned water/contact material or mesh"));return;}
        UStaticMesh* Meshes[]={DrawParts[3]->GetStaticMesh().Get(),Mesh(this,1,Material),Mesh(this,2,Material),Mesh(this,3,Material)};
        for(int32 Kind=0;Kind<4;++Kind)
        {
            if(!Meshes[Kind]){P.EffectsFailed=true;return;}if(Kind>0)OwnedMeshes.Add(Meshes[Kind]);auto* Part=NewObject<UInstancedStaticMeshComponent>(this,FName(*FString::Printf(TEXT("Wildlife_PondEffect_%d"),Kind)));AddInstanceComponent(Part);Part->SetupAttachment(RootComponent);Part->SetIsReplicated(false);Part->SetMobility(EComponentMobility::Movable);Part->SetStaticMesh(Meshes[Kind]);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetGenerateOverlapEvents(false);Part->SetCanEverAffectNavigation(false);Part->SetCastShadow(false);Part->bAffectDistanceFieldLighting=false;Part->bAffectDynamicIndirectLighting=false;Part->SetHasPerInstancePrevTransforms(true);Part->SetCullDistances(0,15000);if(Kind>0)Part->SetNumCustomDataFloats(4);
            P.EffectCurrent[Kind].Init(FTransform(FQuat::Identity,P.HiddenAnchors[0],FVector::ZeroVector),Counts[Kind]);P.EffectPrevious[Kind]=P.EffectCurrent[Kind];P.EffectData[Kind].Init(0.f,Counts[Kind]*4);Part->AddInstances(P.EffectCurrent[Kind],false,false,false);Part->SetVisibility(false);Part->RegisterComponent();PondEffectParts.Add(Part);
        }
        for(int32 Route=0;Route<2;++Route)
        {
            const auto& Path=P.Shore.Routes[Route];for(int32 I=1;I<Path.Points.Num();++I)
            {float A=0.f,B=0.f;if(!HouseCalderWater::WaterAt(FVector2D(Path.Points[I-1]),A)||HouseCalderWater::WaterAt(FVector2D(Path.Points[I]),B))continue;double Lo=Path.Arc[I-1],Hi=Path.Arc[I];for(int32 J=0;J<18;++J){const double Mid=(Lo+Hi)*.5;float W=0.f;if(HouseCalderWater::WaterAt(FVector2D(HouseCalderWildlifeSkyPaths::Along(Path,Mid)),W))Lo=Mid;else Hi=Mid;}P.WaterExitArc[Route]=(Lo+Hi)*.5;}
        }
        for(double& At:P.EffectLastWaterMotion)At=-1.e30;
        P.EffectsReady=true;UE_LOG(LogTemp,Display,TEXT("CALDER_POND_EFFECT_READY components=4 instances=16/32/16/16 authoredTextures=0 contactReuse=1 collision=0 shadows=0 actors=0 lights=0 Niagara=0 nativeAcceptancePending=1"));
    }
    if(!P.EffectsReady||P.EffectsFailed)return;
    struct FView{FVector Eye,Forward;double Cos;};TArray<FView,TInlineAllocator<2>> Views;bool Near=false;
    for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)if(const auto* PC=It->Get())if(PC->IsLocalController()&&PC->PlayerCameraManager)
    {const FVector Eye=PC->PlayerCameraManager->GetCameraLocation();Views.Add({Eye,PC->PlayerCameraManager->GetCameraRotation().Vector(),FMath::Cos(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle()*.5+10.))});for(const auto& Home:P.Plan.Homes)Near|=FVector::DistSquared(Home,Eye)<FMath::Square(4000.);for(int32 R=0;R<2;++R)Near|=FVector::DistSquared(P.Shore.Routes[R].Points.Last(),Eye)<FMath::Square(4000.);}
    const double Key=Near?Now:FMath::FloorToDouble(Now*15.)/15.;if(Key==P.EffectKey)return;P.EffectKey=Key;const double Raw=HouseCalderWildlife::CycleTime(GetWorld());
    bool Changed[4]={};for(int32 Kind=0;Kind<4;++Kind)P.EffectShown[Kind]=0;
    if(Raw<P.EffectClock){for(double& At:P.EffectLastWaterMotion)At=-1.e30;for(bool& Wet:P.EffectWasWater)Wet=false;}P.EffectClock=Raw;
    auto Put=[&](int32 Kind,int32 Slot,const FTransform& Wanted,float Alpha,const FVector& Colour)
    {
        bool Show=Alpha>.001;const FVector At=Wanted.GetTranslation();bool InView=false;
        for(const auto& V:Views){const FVector D=At-V.Eye;InView|=D.SizeSquared()<FMath::Square(15000.)&&(D.SizeSquared()<10000.||FVector::DotProduct(D.GetSafeNormal(),V.Forward)>V.Cos);}Show&=InView;
        const FTransform Root=Show?Wanted:FTransform(FQuat::Identity,P.HiddenAnchors[Slot%2],FVector::ZeroVector);if(Show)++P.EffectShown[Kind];
        const bool RootChanged=!Root.Equals(P.EffectCurrent[Kind][Slot],.00001);const float Data[4]={Show?Alpha:0.f,float(Colour.X),float(Colour.Y),float(Colour.Z)};bool DataChanged=false;
        if(Kind>0)for(int32 I=0;I<4;++I)DataChanged|=P.EffectData[Kind][Slot*4+I]!=Data[I];
        if(!RootChanged&&!DataChanged&&P.EffectPrevious[Kind][Slot].Equals(Root,.00001))return;
        P.EffectPrevious[Kind][Slot]=!Show?Root:P.EffectCurrent[Kind][Slot];P.EffectCurrent[Kind][Slot]=Root;
        if(Kind>0){PondEffectParts[Kind]->SetCustomData(Slot,MakeArrayView(Data,4),false);for(int32 I=0;I<4;++I)P.EffectData[Kind][Slot*4+I]=Data[I];}Changed[Kind]=true;
    };
    const int64 Epoch=FMath::FloorToInt64(Raw/1800.);const int32 Resident=int32((Epoch%2+2)%2);
    for(int32 Id=0;Id<16;++Id)
    {
        HouseCalderWildlifeGoosePaths::FCycleSample Bird;const bool Active=P.Sample(Raw,Id/8,Id%8,Bird);const double Scale=Bird.Bird.Scale;FTransform Contact(FQuat::Identity,Bird.Bird.Position,FVector::ZeroVector);
        if(Active&&Bird.Stand>.999)
        {
            const FTransform ContactKey(Bird.Bird.Direction.Rotation(),Bird.Bird.Position,FVector(Scale));
            if(!P.EffectContactValid[Id]||!ContactKey.Equals(P.EffectContactKeys[Id],.00001))
            {const FVector At=Bird.Bird.Position;const double Z=HouseCalderTerrain::SurfaceZ(At.X,At.Y),DX=(HouseCalderTerrain::SurfaceZ(At.X+10.,At.Y)-HouseCalderTerrain::SurfaceZ(At.X-10.,At.Y))/20.,DY=(HouseCalderTerrain::SurfaceZ(At.X,At.Y+10.)-HouseCalderTerrain::SurfaceZ(At.X,At.Y-10.))/20.;const FVector N=FVector(-DX,-DY,1.).GetSafeNormal();P.EffectContacts[Id]=FTransform(FRotationMatrix::MakeFromXZ(FVector::VectorPlaneProject(Bird.Bird.Direction,N).GetSafeNormal(),N).ToQuat(),FVector(At.X,At.Y,Z+.6),FVector(.45*Scale,.4*Scale,1.));P.EffectContactKeys[Id]=ContactKey;P.EffectContactValid[Id]=true;}
            Contact=P.EffectContacts[Id];
        }
        Put(0,Id,Contact,Active&&Bird.Stand>.999?1.f:0.f,FVector::ZeroVector);
        const bool OnWater=Active&&Bird.OnWater;
        if(OnWater&&P.EffectWasWater[Id]&&FVector::DistSquared(Bird.Bird.Position,P.EffectLastWaterPosition[Id])>.000001)P.EffectLastWaterMotion[Id]=Raw;
        P.EffectWasWater[Id]=OnWater;P.EffectLastWaterPosition[Id]=Bird.Bird.Position;
        // Still birds need no historical path/terrain evaluations. Preserve two
        // seconds of fading trails after motion ends; poses still update fully.
        const bool HasHistory=OnWater&&Raw-P.EffectLastWaterMotion[Id]<2.;
        for(int32 Trail=0;Trail<2;++Trail)
        {
            const int32 Slot=Id*2+Trail;const double Age=.1+Trail*.7+FMath::Fmod(Raw,.7);HouseCalderWildlifeGoosePaths::FCycleSample Old,Before;
            const bool Has=HasHistory&&P.Sample(Raw-Age,Id/8,Id%8,Old)&&P.Sample(Raw-Age-.1,Id/8,Id%8,Before);
            bool Wet=Has&&Old.OnWater&&FVector::Distance(Old.Bird.Position,Before.Bird.Position)>.5;const double Water=Old.WaterSurface;const FVector Centre(Old.Bird.Position.X,Old.Bird.Position.Y,Water);
            if(Wet&&Old.ShoreRoute>=0)
            {
                // A125cm tested envelope safely contains the115cm effect plus
                // at most10cm motion before the next cache refresh.
                if(!P.EffectWetValid[Slot]||FVector::DistSquared(Centre,P.EffectWetCentre[Slot])>100.){P.EffectWetCentre[Slot]=Centre;P.EffectWet[Slot]=WetEnvelope(Centre,125.);P.EffectWetValid[Slot]=true;}
                Wet=P.EffectWet[Slot];
            }
            const FTransform Wake(Old.Bird.Direction.Rotation(),Centre+FVector(0,0,1.6),FVector(1.+Age*.12));Put(1,Slot,Wake,Wet?float(.28*(1.-Age/2.)):0.f,FVector(.25,.28,.27));
        }
        double Birth=-1.e30;bool Small=false;
        if(Id/8!=Resident)Birth=double(Epoch)*1800.+P.Plan.SplitScene+P.Plan.Durations[Id%8];
        if(Id%8>=6&&P.WaterExitArc[Id%8-6]>=0.)
        {
            const double ShoreBirth=P.ShoreSplashBirth(Raw,Id);
            if(ShoreBirth<=Raw&&ShoreBirth>Birth){Birth=ShoreBirth;Small=true;}
        }
        const double Age=Raw-Birth,U=FMath::Clamp(Age/1.2,0.,1.);HouseCalderWildlifeGoosePaths::FCycleSample Splash;float Water=0.f;
        const bool Splashing=Age>=0.&&Age<1.2&&P.Sample(Birth+.001,Id/8,Id%8,Splash)&&HouseCalderWater::WaterAt(FVector2D(Splash.Bird.Position),Water)&&WetEnvelope(FVector(Splash.Bird.Position.X,Splash.Bird.Position.Y,Water),Small?15.:70.);
        const FVector At(Splash.Bird.Position.X,Splash.Bird.Position.Y,double(Water));const FQuat Q=Splash.Bird.Direction.Rotation().Quaternion();
        const double DropScale=Small?.35+.4*U:1.+2.*U,RingScale=Small?.05+.2*U:.25+U;
        Put(2,Id,FTransform(Q,At+FVector(0,0,2.+(Small?8.:18.)*FMath::Sin(PI*U)),FVector(DropScale,DropScale,1.)),Splashing?float(.65*FMath::Square(1.-U)):0.f,FVector(.62,.65,.61));
        Put(3,Id,FTransform(Q,At+FVector(0,0,1.6),FVector(RingScale,RingScale,1.)),Splashing?float(.42*FMath::Square(1.-U)):0.f,FVector(.44,.48,.46));
    }
    for(int32 Kind=0;Kind<4;++Kind)
    {
        if(P.EffectShown[Kind]>0)P.EffectHideAfter[Kind]=Now+5.;const bool Visible=P.EffectShown[Kind]>0||(P.EffectVisible[Kind]&&Now<P.EffectHideAfter[Kind]);if(Visible!=P.EffectVisible[Kind]){PondEffectParts[Kind]->SetVisibility(Visible);P.EffectVisible[Kind]=Visible;}
        if(Changed[Kind])PondEffectParts[Kind]->BatchUpdateInstancesTransforms(0,P.EffectCurrent[Kind],P.EffectPrevious[Kind],false,false,false);
    }
    if(Now>=P.EffectNextBounds){P.EffectNextBounds=Now+.5;for(int32 Kind=0;Kind<4;++Kind)if(Changed[Kind]){PondEffectParts[Kind]->UpdateBounds();PondEffectParts[Kind]->MarkRenderTransformDirty();}}
    if(Now>=P.EffectStatsAt+10.){P.EffectStatsAt=Now;UE_LOG(LogTemp,Display,TEXT("CALDER_POND_EFFECT_FRAME contact=%d wakes=%d drops=%d rings=%d components=4 customFloats=0/4/4/4 physicsQueries=0 nativeAcceptancePending=1"),P.EffectShown[0],P.EffectShown[1],P.EffectShown[2],P.EffectShown[3]);}
}
