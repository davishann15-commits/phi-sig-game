#pragma once
#include "HouseBlackHallMesh.h"

// Keep each shadow caster local. One house-wide non-Nanite procedural primitive
// invalidates shadow pages for distant walls whenever a nearby light is updated.
// Clip with interpolated UVs, rather than assigning long roof triangles by center.
inline TArray<FHallMesh> HouseSpatialMeshes(const FHallMesh& Source, float Cell = 1600.f)
{
    struct FV { FVector P,N; FVector2D UV; FProcMeshTangent T; };
    auto Clip=[](const TArray<FV>& In,int Axis,float Value,bool Greater)
    {
        TArray<FV> Out;
        for(int i=0;i<In.Num();++i)
        {
            const FV& A=In[i];const FV& B=In[(i+1)%In.Num()];
            const double DA=(A.P[Axis]-Value)*(Greater?1:-1), DB=(B.P[Axis]-Value)*(Greater?1:-1);
            if(DA>=0)Out.Add(A);
            if((DA>0 && DB<0)||(DA<0 && DB>0))
            {
                const double Alpha=DA/(DA-DB);FV V=A;
                V.P=FMath::Lerp(A.P,B.P,Alpha); V.P[Axis]=Value;
                V.N=FMath::Lerp(A.N,B.N,Alpha).GetSafeNormal();V.UV=FMath::Lerp(A.UV,B.UV,Alpha);
                Out.Add(V);
            }
        }
        return Out;
    };
    TMap<FIntPoint,FHallMesh> Tiles;
    for(FName Key:Source.Order)
    {
        const FHallSection& S=Source.Sections[Key];
        for(int i=0;i<S.I.Num();i+=3)
        {
            TArray<FV> Tri;FBox B(ForceInit);
            for(int k=0;k<3;++k){int v=S.I[i+k];Tri.Add({S.V[v],S.N[v],S.UV[v],S.T[v]});B+=S.V[v];}
            const int X0=FMath::FloorToInt(B.Min.X/Cell),X1=FMath::FloorToInt(B.Max.X/Cell);
            const int Y0=FMath::FloorToInt(B.Min.Y/Cell),Y1=FMath::FloorToInt(B.Max.Y/Cell);
            for(int x=X0;x<=X1;++x)for(int y=Y0;y<=Y1;++y)
            {
                auto Poly=Clip(Tri,0,x*Cell,true);Poly=Clip(Poly,0,(x+1)*Cell,false);
                Poly=Clip(Poly,1,y*Cell,true);Poly=Clip(Poly,1,(y+1)*Cell,false);
                for(int k=1;k+1<Poly.Num();++k)
                {
                    if(FVector::CrossProduct(Poly[k].P-Poly[0].P,Poly[k+1].P-Poly[0].P).SizeSquared()<1.e-8)continue;
                    FHallSection& D=Tiles.FindOrAdd(FIntPoint(x,y)).Sec(Key);const int v=D.V.Num();
                    for(const FV& P:{Poly[0],Poly[k],Poly[k+1]}){D.V.Add(P.P);D.N.Add(P.N);D.UV.Add(P.UV);D.T.Add(P.T);}
                    D.I.Append({v,v+1,v+2});
                }
            }
        }
    }
    TArray<FHallMesh> Result;for(auto& Pair:Tiles)Result.Add(MoveTemp(Pair.Value));return Result;
}
