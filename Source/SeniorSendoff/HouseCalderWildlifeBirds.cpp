#include "HouseCalderWildlifeBirds.h"
#include "HouseCalderWildlife.h"
#include "HouseBlackHallMesh.h"
#include "HouseCalderCrowData.inl"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace HouseCalderWildlifeBirdsImpl
{
bool GroundLegArt(){static const bool B=HouseCalderWildlife::BirdsEnabled()||FParse::Param(FCommandLine::Get(),TEXT("CalderGooseLegArtTrial"))||FParse::Param(FCommandLine::Get(),TEXT("CalderGooseShoreTrial"))||FParse::Param(FCommandLine::Get(),TEXT("CalderGooseReactionsTrial"))||FParse::Param(FCommandLine::Get(),TEXT("CalderGooseEffectsTrial"))||FParse::Param(FCommandLine::Get(),TEXT("CalderGooseAudioTrial"));return B;}
FName Key(int32 Cell,int32 Joint){return FName(*FString::Printf(TEXT("Bird_%d_%d"),Cell,Joint));}
FVector Pivot(int32 Kind,int32 I)
{
    if(Kind==1)return HouseCalderCrowData::Pivot(I);
    static const FVector Goose[]={FVector::ZeroVector,{12,0,5},{47,0,13},{0,9,5},{-1,37,5},{4,57,6},
        {0,-9,5},{-1,-37,5},{4,-57,6},{-32,0,-1},{-18,5,-10},{-26,5,-12},{-18,-5,-10},{-26,-5,-12},{54,0,12},FVector::ZeroVector};
    static const FVector Bat[]={FVector::ZeroVector,{3,0,1},{5,0,1},{1,2,0},{0,7,0},{4,11,0},
        {1,-2,0},{0,-7,0},{4,-11,0},{-5,0,0},{-4,2,0},{-5,3,0},{-4,-2,0},{-5,-3,0},{6,0,1},FVector::ZeroVector};
    return Kind==0?Goose[I]:Bat[I];
}
void Ellipsoid(FHallMesh& M,const FVector& Centre,const FVector& Radii,int32 Cell,int32 Joint,int32 Lod,int32 Style=0)
{
    const int32 Sides=Lod==0?24:Lod==1?16:10,Rings=Lod==0?14:Lod==1?10:6;
    auto P=[&](int32 R,int32 S){const double A=double(S)/Sides*2*PI,B=-PI*.5+double(R)/Rings*PI;return Centre+FVector(FMath::Cos(B)*FMath::Cos(A),FMath::Cos(B)*FMath::Sin(A),FMath::Sin(B))*Radii;};
    for(int32 R=0;R<Rings;++R)for(int32 S=0;S<Sides;++S)
    {
        const FVector A=P(R,S),B=P(R,S+1),C=P(R+1,S+1),D=P(R+1,S),Mid=(A+B+C+D)*.25;
        const FVector N=((Mid-Centre)/(Radii*Radii)).GetSafeNormal();int32 Paint=Cell;
        if(Style==1 && Mid.Z<Centre.Z-2.)Paint=2;
        // Cheeks/chin wrap the lower sides of the black head, leaving its cap/bill black.
        if(Style==2 && Mid.X>Centre.X-3. && Mid.X<Centre.X+2.5 && Mid.Z<Centre.Z+1.8 && (FMath::Abs(Mid.Y)>2.5 || Mid.Z<Centre.Z-2.))Paint=3;
        if(Style==3 && Mid.X>.5 && Mid.X<3.4 && Mid.Z>.7 && FMath::Abs(Mid.Y)>1.2)Paint=14;
        M.QuadUV(Key(Paint,Joint),A,B,C,D,N,{double(S)/Sides,double(R)/Rings},{double(S+1)/Sides,double(R)/Rings},
            {double(S+1)/Sides,double(R+1)/Rings},{double(S)/Sides,double(R+1)/Rings},FVector::ForwardVector);
        auto& Section=M.Sections[Key(Paint,Joint)];const FVector Corners[]={A,B,C,D};
        for(int32 K=0;K<4;++K)Section.N[Section.N.Num()-4+K]=((Corners[K]-Centre)/(Radii*Radii)).GetSafeNormal();
    }
}
void Tube(FHallMesh& M,const FVector& A,const FVector& B,double R0,double R1,int32 Cell,int32 Joint,int32 Lod)
{
    const FVector D=(B-A).GetSafeNormal(),U=FVector::CrossProduct(D,FMath::Abs(D.Z)>.9?FVector::RightVector:FVector::UpVector).GetSafeNormal(),V=FVector::CrossProduct(D,U);
    const int32 Sides=Lod==0?16:Lod==1?10:6;
    for(int32 S=0;S<Sides;++S)
    {
        const FVector N0=U*FMath::Cos(S*2.*PI/Sides)+V*FMath::Sin(S*2.*PI/Sides),N1=U*FMath::Cos((S+1)*2.*PI/Sides)+V*FMath::Sin((S+1)*2.*PI/Sides);
        M.QuadUV(Key(Cell,Joint),A+N0*R0,A+N1*R0,B+N1*R1,B+N0*R1,(N0+N1).GetSafeNormal(),{double(S)/Sides,0},{double(S+1)/Sides,0},{double(S+1)/Sides,1},{double(S)/Sides,1},D);
        auto& Section=M.Sections[Key(Cell,Joint)];const FVector Slope=D*((R1-R0)/FVector::Distance(A,B));
        const FVector Smooth0=(N0-Slope).GetSafeNormal(),Smooth1=(N1-Slope).GetSafeNormal();const FVector Normals[]={Smooth0,Smooth1,Smooth1,Smooth0};
        for(int32 K=0;K<4;++K)Section.N[Section.N.Num()-4+K]=Normals[K];
    }
}
void Leaf(FHallMesh& M,const FVector& A,const FVector& B,double Width,double Thickness,int32 Cell,int32 Joint,int32 Lod)
{
    const FVector D=(B-A).GetSafeNormal(),U=FVector::CrossProduct(FVector::UpVector,D).GetSafeNormal();
    const int32 Rings=Lod==0?8:Lod==1?5:3;
    for(int32 I=0;I<Rings;++I)for(int32 Side=0;Side<2;++Side)
    {
        auto P=[&](int32 R,double Sign){const double T=double(R)/Rings,W=Width*FMath::Pow(FMath::Max(0.,FMath::Sin(PI*T)),.6);return FMath::Lerp(A,B,T)+U*W*Sign+FVector(0,0,Side?Thickness:-Thickness);};
        M.QuadUV(Key(Cell,Joint),P(I,-1),P(I,1),P(I+1,1),P(I+1,-1),Side?FVector::UpVector:-FVector::UpVector,{0,double(I)/Rings},{1,double(I)/Rings},{1,double(I+1)/Rings},{0,double(I+1)/Rings},D);
    }
    for(int32 Sign:{-1,1})for(int32 I=0;I<Rings;++I)
    {
        auto Edge=[&](int32 R,double Z){const double T=double(R)/Rings;return FMath::Lerp(A,B,T)+U*(Sign*Width*FMath::Pow(FMath::Max(0.,FMath::Sin(PI*T)),.6))+FVector(0,0,Z);};
        M.QuadUV(Key(Cell,Joint),Edge(I,-Thickness),Edge(I+1,-Thickness),Edge(I+1,Thickness),Edge(I,Thickness),U*Sign,{0,0},{1,0},{1,1},{0,1},D);
    }
}
void Goose(FHallMesh& M,int32 Lod)
{
    Ellipsoid(M,{-8,0,0},{27,13,14},0,0,Lod,1);
    Tube(M,{12,0,5},{30,0,10},5.3,3.5,1,1,Lod);Tube(M,{30,0,10},{47,0,13},3.5,3.6,1,1,Lod);
    Ellipsoid(M,{50,0,13},{8.5,5.2,6},1,2,Lod,2);Ellipsoid(M,{60,0,12.4},{7,3.1,.8},5,2,Lod);Ellipsoid(M,{60,0,11.1},{7,2.9,.7},5,14,Lod);
    for(int32 Sign:{-1,1})
    {
        Ellipsoid(M,{52.4,Sign*4.88,15.4},{.65,.30,.65},11,2,Lod);
        const int32 Joint=Sign>0?3:6;auto V=[Sign](double X,double Y,double Z){return FVector(X,Y*Sign,Z);};
        Ellipsoid(M,V(-4,19,5),{12,18,1.6},0,Joint,Lod);
        Ellipsoid(M,V(-4,43,5),{11,15,1.1},4,Joint+1,Lod);
        const int32 Feathers=Lod==2?6:10;
        for(int32 I=0;I<Feathers;++I)
        {
            const double T=double(I)/(Feathers-1);
            Leaf(M,V(3-14*T,49+7*T,5.5),V(10-29*T,76-5*T,4+T),2.8,.24,5,Joint+2,Lod);
        }
        for(int32 I=0;I<(Lod==2?5:9);++I)
            Leaf(M,V(-6,15+I*4,3.2),V(-18+I*.5,17+I*4,3.0),2.8,.20,4,Joint+(I>=5?1:0),Lod);
        if(GroundLegArt())
        {
            Tube(M,V(-18,5,-10),V(-26,5,-12),1.2,.9,5,Sign>0?10:12,Lod);
            Tube(M,V(-26,5,-12),V(-26,5,-22),.9,.75,5,Sign>0?11:13,Lod);
            Leaf(M,V(-26,5,-22),V(-16,5,-22),4.,.25,5,Sign>0?11:13,Lod);
        }
        else
        {
            Tube(M,V(-18,5,-10),V(-28,5,-12),1.2,1.,5,Sign>0?10:12,Lod);
            Leaf(M,V(-27,5,-12),V(-37,5,-12),2.4,.35,5,Sign>0?11:13,Lod);
        }
    }
    Ellipsoid(M,{-29,0,-1},{9,9,6},3,9,Lod);
    for(int32 I=0;I<6;++I)Leaf(M,{-30,(I-2.5)*2.,0},{-46,(I-2.5)*2.8,-1},2.,.22,5,9,Lod);
}
void Bat(FHallMesh& M,int32 Kind,int32 Lod)
{
    const int32 Fur=Kind==3?7:6,Membrane=Kind==3?9:8;
    Ellipsoid(M,{-1,0,0},{5.8,2.5,2.7},Fur,0,Lod,Kind==3?3:0);Ellipsoid(M,{4.5,0,1.2},{2.3,1.8,1.8},Fur,2,Lod);
    Ellipsoid(M,{6.5,0,.7},{1.3,1.4,.85},10,2,Lod);
    for(int32 Sign:{-1,1})
    {
        Ellipsoid(M,{4,Sign*1.5,2.9},{.85,.38,1.1},Kind==3?Fur:10,2,Lod);
        Ellipsoid(M,{5.8,Sign*1.6,1.8},{.35,.19,.35},11,2,Lod);
        const int32 J=Sign>0?3:6;auto V=[Sign](double X,double Y,double Z){return FVector(X,Y*Sign,Z);};
        Tube(M,V(1,2,0),V(0,7,0),.38,.26,Fur,J,Lod);Tube(M,V(0,7,0),V(4,11,0),.26,.19,Fur,J+1,Lod);
        // Four fingers carry a scalloped membrane, with the free thumb at the wrist.
        const FVector Tips[]={V(6,18,0),V(0,17,-.4),V(-6,14,-.6),V(-7,8,-.5),V(-5,1.4,-1),V(1,2,0),V(0,7,.4)};
        for(int32 I=0;I<6;++I)
        {
            if(I<4)Tube(M,V(4,11,0),Tips[I],.15,.06,Fur,J+2,Lod);
            const FVector A=V(4,11,1),B=Tips[I],D=Tips[I+1],C=(B+D)*.5+(A-(B+D)*.5)*.18;
            for(int32 Side:{-1,1})M.QuadUV(Key(Membrane,J+2),A+FVector(0,0,Side*.04),B+FVector(0,0,Side*.04),C+FVector(0,0,Side*.04),D+FVector(0,0,Side*.04),FVector(0,0,Side),{.5,.2},{0,1},{.5,.8},{1,1},FVector::ForwardVector);
        }
        Tube(M,V(4,11,0),V(5.4,10.5,0),.19,.10,Fur,J+2,Lod);
        Tube(M,V(-4,2,0),V(-5,3,0),.32,.18,Fur,Sign>0?10:12,Lod);
    }
    Leaf(M,{-3,0,0},{-8,0,0},3.1,.06,Membrane,9,Lod);
}
void Describe(const FHallMesh& From,FMeshDescription& D)
{
    // Keep UV-element channels and the vertex-instance attribute in sync.
    // The editor's mesh-description commit re-registers from the element count.
    D.SetNumUVChannels(5);
    FStaticMeshAttributes A(D);A.Register();auto P=A.GetVertexPositions();auto N=A.GetVertexInstanceNormals();auto T=A.GetVertexInstanceTangents();auto Signs=A.GetVertexInstanceBinormalSigns();auto UV=A.GetVertexInstanceUVs();auto Colours=A.GetVertexInstanceColors();UV.SetNumChannels(5);
    const auto Group=D.CreatePolygonGroup();A.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("CalderBirdPart");
    for(const FName Name:From.Order)
    {
        TArray<FString> Tokens;Name.ToString().ParseIntoArray(Tokens,TEXT("_"));check(Tokens.Num()==3);const int32 Cell=FCString::Atoi(*Tokens[1]),Joint=FCString::Atoi(*Tokens[2]);
        const auto& S=From.Sections[Name];TArray<FVertexID> Vertices;
        for(const FVector& V:S.V){const auto Id=D.CreateVertex();P[Id]=FVector3f(V);Vertices.Add(Id);}
        for(int32 I=0;I<S.I.Num();I+=3)
        {
            // QuadUV's first triangle is degenerate at an ellipsoid pole.
            // Orient each actual triangle from its own area and smooth normals.
            const int32 A0=S.I[I],A1=S.I[I+1],A2=S.I[I+2];
            const bool Reverse=FVector::DotProduct(FVector::CrossProduct(S.V[A1]-S.V[A0],S.V[A2]-S.V[A0]),S.N[A0]+S.N[A1]+S.N[A2])>0.;
            const int32 Indices[]={A0,Reverse?A2:A1,Reverse?A1:A2};
            TArray<FVertexInstanceID,TInlineAllocator<3>> Triangle;
            for(int32 K=0;K<3;++K)
            {
                const int32 J=Indices[K];const auto Id=D.CreateVertexInstance(Vertices[J]);N[Id]=FVector3f(S.N[J]);T[Id]=FVector3f(S.T[J].TangentX);Signs[Id]=S.T[J].bFlipTangentY?-1.f:1.f;Colours[Id]=FVector4f(1,1,1,1);
                const FVector2D Local=S.UV[J];UV.Set(Id,0,FVector2f((Cell%4+(2.+60.*FMath::Clamp(Local.X,0.,1.))/64.)/4.,(Cell/4+(2.+60.*FMath::Clamp(Local.Y,0.,1.))/64.)/4.));
                for(int32 Channel=1;Channel<5;++Channel)UV.Set(Id,Channel,FVector2f(0,0));
                if((Cell==8 || Cell==9) && (Joint==5 || Joint==8))
                {
                    // The skin attaches to the torso, then follows shoulder,
                    // elbow and wrist; the inner membrane must not ride the wrist.
                    const double Span=FMath::Abs(S.V[J].Y);const int32 Base=Joint==5?3:6;
                    int32 FromJoint=0,ToJoint=Base;double Blend=FMath::Clamp((Span-2.)/3.,0.,1.);
                    if(Span>=5.){FromJoint=Base;ToJoint=Base+1;Blend=FMath::Clamp((Span-5.)/3.,0.,1.);}
                    if(Span>=8.){FromJoint=Base+1;ToJoint=Base+2;Blend=FMath::Clamp((Span-8.)/3.,0.,1.);}
                    Blend=Blend*Blend*(3.-2.*Blend);UV.Set(Id,1,FVector2f(float(FromJoint),float(1.-Blend)));UV.Set(Id,2,FVector2f(float(ToJoint),float(Blend)));
                }
                else UV.Set(Id,1,FVector2f(float(Joint),1.f));Triangle.Add(Id);
            }
            D.CreateTriangle(Group,Triangle);
        }
    }
}
}
UStaticMesh* HouseCalderWildlifeBirds::Build(UObject* Outer,int32 Kind,UMaterialInterface* Material)
{
    if(Kind==1)return LoadObject<UStaticMesh>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Wildlife/SM_CalderCrowBody.SM_CalderCrowBody"));
    auto* Mesh=NewObject<UStaticMesh>(Outer,NAME_None,RF_Transient);Mesh->bAllowCPUAccess=true;
    const FName Slot(TEXT("CalderBirdPart"));Mesh->GetStaticMaterials().Add(FStaticMaterial(Material,Slot
#if WITH_EDITOR
        ,Slot
#endif
    ));
    TArray<FMeshDescription> Descriptions;Descriptions.SetNum(3);TArray<const FMeshDescription*> Sources;
    for(int32 Lod=0;Lod<3;++Lod){FHallMesh M;if(Kind==0)HouseCalderWildlifeBirdsImpl::Goose(M,Lod);else HouseCalderWildlifeBirdsImpl::Bat(M,Kind,Lod);HouseCalderWildlifeBirdsImpl::Describe(M,Descriptions[Lod]);Sources.Add(&Descriptions[Lod]);}
    UStaticMesh::FBuildMeshDescriptionsParams Params;Params.bFastBuild=true;Params.bAllowCpuAccess=true;Params.bBuildSimpleCollision=false;Params.bCommitMeshDescription=false;Params.PerLODOverrides.SetNum(3);
    for(auto& Lod:Params.PerLODOverrides){Lod.bUseFullPrecisionUVs=true;Lod.bUseHighPrecisionTangentBasis=true;}
    Mesh->BuildFromMeshDescriptions(Sources,Params);
    auto* Data=Mesh->GetRenderData();if(!Data || Data->LODResources.Num()!=3)return nullptr;
    for(int32 I=0;I<3;++I)Data->ScreenSize[I].Default=I==0?1.f:I==1?.3f:.08f;Mesh->SetPositiveBoundsExtension(FVector(200));Mesh->SetNegativeBoundsExtension(FVector(200));return Mesh;
}
double HouseCalderWildlifeBirds::Frequency(int32 Kind){return Kind==0?3.6:Kind==1?5.4:8.4;}
void HouseCalderWildlifeBirds::Pose(int32 Kind,double Seconds,const FTransform& Root,TArray<FTransform>& Controls,double Mouth,double Flap,double Fold,double Neck)
{
    static const int32 Parents[]={-1,0,1,0,3,4,0,6,7,0,0,10,0,12,2,0};
    const double Phase=Seconds*Frequency(Kind)*2*PI,Amplitude=Kind==0?.72:Kind==1?.92:1.02;
    Flap=FMath::Clamp(Flap,0.,1.);Fold=FMath::Clamp(Fold,0.,1.);Neck=FMath::Clamp(Neck,0.,1.);
    Controls.SetNum(16);FTransform Local[16];
    for(int32 I=0;I<16;++I)
    {
        FQuat Q=FQuat::Identity;
        if(I>=3 && I<=8)
        {const int32 Segment=(I-3)%3;const double Sign=I<6?1.:-1.;Q=FQuat(FVector::ForwardVector,Sign*(Segment==0?Amplitude*FMath::Sin(Phase):Segment==1?.18*FMath::Sin(Phase+.5):.13*FMath::Sin(Phase+1.))*(Kind==0?Flap*(1.-Fold):1.));
}
        if(I==1 || I==2)Q=FQuat(FVector::RightVector,.025*FMath::Sin(Phase*.25+I));
        if(Kind==0 && (I==1 || I==2))Q=FQuat(FVector::RightVector,FMath::Lerp(.025*FMath::Sin(Phase*.25+I),.045*FMath::Sin(Seconds*(2.*PI*167./1800.)+I),Fold)+(I==1?-1.10:1.10)*Neck);
        if(I==9)Q=FQuat(FVector::RightVector,Kind==0?FMath::Lerp(.06*FMath::Sin(Phase-.4),.018*FMath::Sin(Seconds*(2.*PI*192./1800.)),Fold):.06*FMath::Sin(Phase-.4));
        if(Kind==1 && (I==10 || I==12))Q=FQuat(FVector::RightVector,2.0);
        if(Kind==1 && (I==11 || I==13))Q=FQuat(FVector::RightVector,-.65);
        if(Kind==1 && I==14)Q=FQuat(FVector::RightVector,FMath::Lerp(-.61,0.,FMath::Clamp(Mouth,0.,1.)));
        if(Kind==0 && I==14)Q=FQuat(FVector::RightVector,.20*FMath::Clamp(Mouth,0.,1.));
        const FVector P=HouseCalderWildlifeBirdsImpl::Pivot(Kind,I);Local[I]=FTransform(Q,P-Q.RotateVector(P));if(Parents[I]>=0)Local[I]=Local[I]*Local[Parents[I]];
        Controls[I]=Local[I]*Root;
    }
    if(Kind==0 && HouseCalderWildlifeBirdsImpl::GroundLegArt())for(int32 I:{11,13})
    {
        const FVector P=HouseCalderWildlifeBirdsImpl::Pivot(0,I);const FQuat Q(FVector::RightVector,PI*(1.-Fold));Controls[I]=FTransform(Q,P-Q.RotateVector(P))*Root;
    }
    if(Kind==0 && Fold>0.)for(int32 I=3;I<=8;++I)
    {
        const int32 Segment=(I-3)%3;const double Sign=I<6?1.:-1.;
        // Stow upper coverts, forearm coverts and primaries along the mantle.
        // A pure hinge chain left the long secondary feathers projecting out.
        const FVector Targets[]={FVector(-4,Sign*6,12),FVector(-14,Sign*4,12.5),FVector(-11,Sign*8.5,13)};
        const FQuat Q(FVector::UpVector,Sign*PI*.5);const FVector P=HouseCalderWildlifeBirdsImpl::Pivot(0,I);
        const FTransform Stowed=FTransform(Q,Targets[Segment]-Q.RotateVector(P))*Root;
        FTransform Blended;Blended.Blend(Controls[I],Stowed,Fold);Controls[I]=Blended;
    }
}
void HouseCalderWildlifeBirds::GroundLegs(const FTransform& Root,const FVector* Soles,const FVector* Normals,TArray<FTransform>& Controls,const FVector* Forwards)
{
    check(Controls.Num()==16);const double Scale=Root.GetScale3D().X;
    for(int32 Leg=0;Leg<2;++Leg)
    {
        const double Sign=Leg==0?1.:-1.;const FVector Normal=Normals[Leg].GetSafeNormal(),Forward=FVector::VectorPlaneProject(Forwards?Forwards[Leg]:Root.GetRotation().GetForwardVector(),Normal).GetSafeNormal();
        const FQuat FootQ=FRotationMatrix::MakeFromXZ(Forward,Normal).ToQuat();const FVector LocalSole(-21.,Sign*5.,-22.25),A(-18.,Sign*5.,-10.),B(-26.,Sign*5.,-12.);
        const FTransform Foot(FootQ,Soles[Leg]-FootQ.RotateVector(LocalSole*Scale),Root.GetScale3D());const FVector Ankle=Foot.TransformPosition(B),WantHip=Root.TransformPosition(A),Direction=(Ankle-WantHip).GetSafeNormal();
        const FVector Hip=Ankle-Direction*(B-A).Size()*Scale;const FQuat UpperQ=FQuat::FindBetweenNormals(Root.GetRotation().RotateVector((B-A).GetSafeNormal()),Direction)*Root.GetRotation();
        Controls[Leg==0?10:12]=FTransform(UpperQ,Hip-UpperQ.RotateVector(A*Scale),Root.GetScale3D());Controls[Leg==0?11:13]=Foot;
    }
}
void HouseCalderWildlifeBirds::Pack(const TArray<FTransform>& Controls,float* Data)
{
    for(int32 I=0;I<16;++I){const auto& T=Controls[I];const FQuat Q=T.GetRotation().GetNormalized();const FVector P=T.GetTranslation();Data[I*7]=float(Q.X);Data[I*7+1]=float(Q.Y);Data[I*7+2]=float(Q.Z);Data[I*7+3]=float(Q.W);Data[I*7+4]=float(P.X);Data[I*7+5]=float(P.Y);Data[I*7+6]=float(P.Z);}
}
