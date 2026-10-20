#include "HouseCalderHedgerowGeometry.h"
#include "HouseBlackHallMesh.h"

namespace HouseCalderHedgerowImpl
{
const FName CaneKey(TEXT("CalderHedgeCanes")), CedarKey(TEXT("CalderHedgeCedar"));
const FName RoseKey(TEXT("CalderHedgeRoseLeaf")), BriarKey(TEXT("CalderHedgeBriarLeaf")), HipKey(TEXT("CalderHedgeHips")), SprayKey(TEXT("CalderHedgeCedarSpray"));

void Triangle(FHallMesh& Mesh,FName Key,const FVector& A,const FVector& B,const FVector& C,
    const FVector& NA,const FVector& NB,const FVector& NC,const FVector2D& UA,const FVector2D& UB,const FVector2D& UC)
{
    if (FVector::CrossProduct(B-A,C-A).SizeSquared()<1e-12) return;
    FHallSection& Section=Mesh.Sec(Key);const int32 Base=Section.V.Num();
    Section.V.Append({A,B,C});Section.N.Append({NA,NB,NC});Section.UV.Append({UA,UB,UC});
    const FVector Tangent=(B-A).GetSafeNormal();
    Section.T.Append({FProcMeshTangent(Tangent,false),FProcMeshTangent(Tangent,false),FProcMeshTangent(Tangent,false)});
    if (FVector::DotProduct(FVector::CrossProduct(B-A,C-A),NA+NB+NC)<0) Section.I.Append({Base,Base+1,Base+2});
    else Section.I.Append({Base,Base+2,Base+1});
}

void Leaf(FHallMesh& Mesh,FName Key,const FVector& Base,const FVector& Direction,double Length,double HalfWidth,int32 LOD,bool Serrate)
{
    const FVector D=Direction.GetSafeNormal(),Side=FVector::CrossProduct(D,FVector::UpVector).GetSafeNormal();
    const int32 Steps=LOD==0?10:LOD==1?4:2;
    auto P=[&](double T,double U)
    {
        const double Teeth=Serrate?1.+.065*FMath::Sin(T*12*PI):1.;
        const double W=HalfWidth*FMath::Sin(PI*FMath::Pow(FMath::Clamp(T,0.,1.),.86))*Teeth;
        return Base+D*(Length*T)+Side*(U*W)+FVector(0,0,FMath::Sin(PI*T)*(1-U*U)*.18);
    };
    auto N=[&](double T,double U)
    {
        return FVector::CrossProduct(P(T+.001,U)-P(T-.001,U),P(T,U+.001)-P(T,U-.001)).GetSafeNormal();
    };
    for (int32 I=0;I<Steps;++I) for (int32 Half=0;Half<2;++Half)
    {
        const double T0=double(I)/Steps,T1=double(I+1)/Steps,U0=Half-1.,U1=Half;
        const FVector V[]={P(T0,U0),P(T0,U1),P(T1,U1),P(T1,U0)};
        FVector Normals[]={N(T0,U0),N(T0,U1),N(T1,U1),N(T1,U0)};
        const FVector Fallback=FVector::CrossProduct(V[2]-V[0],V[3]-V[1]).GetSafeNormal();
        for (FVector& Normal:Normals) if (Normal.IsNearlyZero()) Normal=Fallback;
        const FVector2D UV[]={FVector2D((U0+1)*.5,T0),FVector2D((U1+1)*.5,T0),FVector2D((U1+1)*.5,T1),FVector2D((U0+1)*.5,T1)};
        for (int32 Sign:{1})
        {
            Triangle(Mesh,Key,V[0],V[1],V[2],Normals[0]*Sign,Normals[1]*Sign,Normals[2]*Sign,UV[0],UV[1],UV[2]);
            Triangle(Mesh,Key,V[0],V[2],V[3],Normals[0]*Sign,Normals[2]*Sign,Normals[3]*Sign,UV[0],UV[2],UV[3]);
        }
    }
}

void Hip(FHallMesh& Mesh,const FVector& Centre,int32 LOD)
{
    const int32 Sides=LOD==0?8:LOD==1?5:4,Rings=LOD==0?4:3;
    auto P=[&](int32 Y,int32 X)
    {
        const double T=PI*Y/Rings,A=2*PI*X/Sides;
        return Centre+FVector(.31*FMath::Sin(T)*FMath::Cos(A),.31*FMath::Sin(T)*FMath::Sin(A),.42*FMath::Cos(T));
    };
    auto Add=[&](const FVector& A,const FVector& B,const FVector& C)
    {
        Triangle(Mesh,HipKey,A,B,C,(A-Centre).GetSafeNormal(),(B-Centre).GetSafeNormal(),(C-Centre).GetSafeNormal(),{0,0},{1,0},{.5,1});
    };
    for (int32 Y=0;Y<Rings;++Y) for (int32 X=0;X<Sides;++X)
    {
        if (Y==0) Add(P(0,X),P(1,X),P(1,X+1));
        else if (Y==Rings-1) Add(P(Y,X),P(Y+1,X),P(Y,X+1));
        else {Add(P(Y,X),P(Y+1,X),P(Y+1,X+1));Add(P(Y,X),P(Y+1,X+1),P(Y,X+1));}
    }
    Mesh.Cylinder(CaneKey,Centre+FVector(0,0,.35),Centre+FVector(0,0,.65),.10f,3,FHallUV::World(8),false);
}

void CompoundLeaf(FHallMesh& Mesh,FName Key,const FVector& Base,const FVector& Direction,int32 LOD,bool Rose)
{
    const FVector D=Direction.GetSafeNormal(),S=FVector::CrossProduct(D,FVector::UpVector).GetSafeNormal();
    const int32 Pairs=Rose?3:1;const double Length=Rose?13.:16.;
    Mesh.Cylinder(CaneKey,Base,Base+D*Length,.09f,3,FHallUV::World(20),false);
    for (int32 I=0;I<Pairs;++I) for (int32 Sign:{-1,1})
    {
        const FVector At=Base+D*(3.+I*3.);
        Leaf(Mesh,Key,At,(D*.30+S*Sign+FVector(0,0,.12)).GetSafeNormal(),Rose?3.4:7.5,Rose?1.15:2.7,LOD,true);
    }
    Leaf(Mesh,Key,Base+D*(Length-1.5),D,Rose?4.2:8.0,Rose?1.4:3.0,LOD,true);
}

void Cedar(FHallMesh& Mesh,int32 LOD,TArray<FVector>* Cedars)
{
    const FVector Root(-30,25,-2);
    Mesh.Cylinder(CaneKey,Root,Root+FVector(0,0,180),1.8f,LOD==0?7:4,FHallUV::World(60));
    const int32 Tiers=5,Rays=6;
    for (int32 Level=0;Level<Tiers;++Level) for (int32 Ray=0;Ray<Rays;++Ray)
    {
        const double H=25.+Level*145./Tiers,A=Ray*2*PI/Rays+Level*1.3;
        const FVector D(FMath::Cos(A),FMath::Sin(A),0),S(-D.Y,D.X,0);
        const FVector Base=Root+FVector(0,0,H),End=Base+D*(70.-H*.25)+FVector(0,0,18);
        Mesh.Cylinder(CaneKey,Base,End,.42f,LOD==0?5:3,FHallUV::World(30),false);
        for (int32 Fan=0;Fan<8;++Fan)
        {
            const double FanAngle=Fan*2.399963;
            const FVector Axis=(D*.7+S*(FMath::Sin(FanAngle)*.65)+FVector(0,0,.15+FMath::Cos(FanAngle)*.25)).GetSafeNormal();
            const FVector Across=FVector::CrossProduct(Axis,FVector::UpVector).GetSafeNormal();
            const FVector Origin=FMath::Lerp(Base,End,.16+.78*(Fan+.35)/8.);
            const FVector N=FVector::CrossProduct(Across,Axis).GetSafeNormal();
            const int32 Strips=LOD==0?4:2;
            for (int32 Strip=0;Strip<Strips;++Strip)
            {
                const double T0=double(Strip)/Strips,T1=double(Strip+1)/Strips;
                auto Point=[&](double T,double U){return Origin+Axis*(T*24)+Across*(U*7)+N*(FMath::Sin(T*PI)*1.2);};
                const FVector A0=Point(T0,-1),A1=Point(T0,1),A2=Point(T1,1),A3=Point(T1,-1);
                Triangle(Mesh,SprayKey,A0,A1,A2,N,N,N,{0,1-T0},{1,1-T0},{1,1-T1});
                Triangle(Mesh,SprayKey,A0,A2,A3,N,N,N,{0,1-T0},{1,1-T1},{0,1-T1});
            }
        }
        const int32 Branchlets=LOD==0?6:LOD==1?4:3;
        for (int32 K=0;K<Branchlets;++K)
        {
            const double Along=.22+.7*K/Branchlets;
            const FVector Start=FMath::Lerp(Base,End,Along),Tip=Start+(D*.45+S*(K%2?1:-1))*(10.+K*1.5)+FVector(0,0,5);
            // Dense green, scale-clad sprays retain their silhouette at distance.
            Mesh.Cylinder(CedarKey,Start,Tip,.24f,LOD==0?5:3,FHallUV::World(4),false);
            const FVector Axis=(Tip-Start).GetSafeNormal();
            const FVector U=FVector::CrossProduct(Axis,FVector::UpVector).GetSafeNormal(),V=FVector::CrossProduct(Axis,U);
            const int32 Sprays=LOD==0?3:2;
            for (int32 Spray=0;Spray<Sprays;++Spray)
            {
                const double AlongSpray=.28+Spray*.23;
                const FVector SprayStart=FMath::Lerp(Start,Tip,AlongSpray);
                const FVector SprayAxis=(Axis*.65+U*(Spray%2?-.75:.75)+V*.16).GetSafeNormal();
                const FVector SprayTip=SprayStart+SprayAxis*(7.+K*.7);
                Mesh.Cylinder(CedarKey,SprayStart,SprayTip,.21f,LOD==0?5:3,FHallUV::World(3),false);
                if (LOD!=0 || K!=Branchlets-1 || Spray!=1) continue;
                const FVector SU=FVector::CrossProduct(SprayAxis,FVector::UpVector).GetSafeNormal(),SV=FVector::CrossProduct(SprayAxis,SU);
                const int32 Rows=FMath::CeilToInt((SprayTip-SprayStart).Size()/.18);
                // Overlapping opposite 1.9mm scales cover the whole branchlet.
                for (int32 Row=0;Row<Rows;++Row) for (int32 Scale=0;Scale<4;++Scale)
                {
                    const double Angle=Scale*PI*.5+(Row%2?PI*.5:0);
                    const FVector Across=SU*FMath::Cos(Angle)+SV*FMath::Sin(Angle);
                    const FVector Mid=FMath::Lerp(SprayStart,SprayTip,double(Row)/Rows)+Across*.18;
                    const FVector Side=FVector::CrossProduct(SprayAxis,Across)*.085;
                    const FVector A0=Mid-Side,A1=Mid+Side,A2=Mid+SprayAxis*.19+Across*.07;
                    const FVector Normal=Across.GetSafeNormal();
                    Triangle(Mesh,CedarKey,A0,A1,A2,Normal,Normal,Normal,{0,0},{1,0},{.5,1});
                    
                    if (Cedars && Row==Rows-3 && Scale==0) Cedars->Add((A0+A1+A2)/3.);
                }
            }
        }
    }
}

void Geometry(FHallMesh& Mesh,int32 LOD,TArray<FVector>* Hips,TArray<FVector>* Cedars)
{
    Cedar(Mesh,LOD,Cedars);
    const int32 Segments=LOD==0?12:LOD==1?7:4;
    for (int32 Cane=0;Cane<5;++Cane)
    {
        const double Angle=Cane*2.39996;
        const FVector D(FMath::Cos(Angle),FMath::Sin(Angle),0),Root(24,0,-2);
        auto Curve=[&](double T){return Root+D*(105*T)+FVector(0,0,205*FMath::Sin(T*PI*.65));};
        auto P=[&](double T){const double Q=FMath::Clamp(T,0.,1.)*Segments;const int32 I=FMath::Min(FMath::FloorToInt(Q),Segments-1);return FMath::Lerp(Curve(double(I)/Segments),Curve(double(I+1)/Segments),Q-I);};
        for (int32 K=0;K<Segments;++K)
        {
            const double T0=double(K)/Segments,T1=double(K+1)/Segments;
            Mesh.Cylinder(CaneKey,P(T0),P(T1),float(.7-.4*T0),LOD==0?5:3,FHallUV::World(25),false);
        }
        const int32 Nodes=LOD==0?7:LOD==1?4:2;
        for (int32 K=0;K<Nodes;++K)
        {
            const double T=.25+.68*K/Nodes;
            const FVector At=P(T),Side(-D.Y,D.X,0),LeafDirection=(Side*(K%2?1:-1)+D*.25+FVector(0,0,.18)).GetSafeNormal();
            CompoundLeaf(Mesh,RoseKey,At,LeafDirection,LOD,true);
            if (LOD==0)
            {
                const FVector Thorn=At-Side*.5;
                const FVector Normal=FVector::CrossProduct(D,FVector::UpVector).GetSafeNormal();
                Triangle(Mesh,CaneKey,Thorn,Thorn+D*.9,Thorn-D*.6+FVector(0,0,.65),Normal,Normal,Normal,{0,0},{1,0},{.5,1});
            }
            if (K%2==0)
            {
                const FVector Branch=At+LeafDirection*6+FVector(0,0,1),Centre=Branch+LeafDirection*2+FVector(0,0,-1.2);
                Mesh.Cylinder(CaneKey,At,Branch,.10f,3,FHallUV::World(10),false);
                Mesh.Cylinder(CaneKey,Branch,Centre+FVector(0,0,.3),.07f,3,FHallUV::World(10),false);
                Hip(Mesh,Centre,LOD);if (Hips) Hips->Add(Centre);
            }
        }
    }
    for (int32 Cane=0;Cane<3;++Cane)
    {
        const double A=Cane*2.1+.4;const FVector D(FMath::Cos(A),FMath::Sin(A),0),Root(0,-30,-2);
        auto Curve=[&](double T){return Root+D*(125*T)+FVector(0,0,155*FMath::Sin(T*PI*.83));};
        auto P=[&](double T){const double Q=FMath::Clamp(T,0.,1.)*Segments;const int32 I=FMath::Min(FMath::FloorToInt(Q),Segments-1);return FMath::Lerp(Curve(double(I)/Segments),Curve(double(I+1)/Segments),Q-I);};
        for (int32 K=0;K<Segments;++K) Mesh.Cylinder(CaneKey,P(double(K)/Segments),P(double(K+1)/Segments),.55f,LOD==0?5:3,FHallUV::World(30),false);
        const int32 Nodes=LOD==0?6:LOD==1?3:2;
        for (int32 K=0;K<Nodes;++K)
        {
            const FVector At=P(.18+.72*K/Nodes),Side(-D.Y,D.X,0);
            CompoundLeaf(Mesh,BriarKey,At,(Side*(K%2?1:-1)+D*.2+FVector(0,0,.2)).GetSafeNormal(),LOD,false);
        }
    }
}
}
