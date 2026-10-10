#include "HouseExteriorBrick.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallLook.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace HouseBrick
{
namespace
{
FName FaceKey(const FVector& P)
{
    const int32 K=FMath::Abs(FMath::RoundToInt(P.X*1.73f+P.Y*.53f+P.Z*.89f))%5;
    return *FString::Printf(TEXT("EX_BrickFace%d"),K);
}

// A recessed backing plane, a real bevel, and a slightly irregular raised face.
void Brick(FHallMesh& M,const FVector& A,const FVector& B,const FVector& C,const FVector& D,const FVector& N)
{
    const FVector Centre=(A+B+C+D)*.25f;
    const float Depth=.90f+.32f*FMath::Sin(Centre.X*.38f+Centre.Z*.63f);
    const FVector Rim[4]={A+N*.08f,B+N*.08f,C+N*.08f,D+N*.08f};
    FVector Front[4];
    for(int32 K=0;K<4;++K)Front[K]=FMath::Lerp(Rim[K],Centre+N*.08f,.045f)+N*Depth;
    const FName Key=FaceKey(Centre);
    M.QuadUV(Key,Front[0],Front[1],Front[2],Front[3],N,
        FVector2D(1,1),FVector2D(1,0),FVector2D(0,0),FVector2D(0,1),(Front[3]-Front[0]).GetSafeNormal());
    for(int32 K=0;K<4;++K)
    {
        const int32 J=(K+1)%4;
        const FVector Edge=((Rim[K]+Rim[J])*.5f-Centre).GetSafeNormal();
        M.Quad(Key,Rim[K],Rim[J],Front[J],Front[K],(N+Edge*.45f).GetSafeNormal(),FHallUV::World(25.f));
    }
}
}

void CreateLook(UHouseBlackHallSubsystem* L)
{
    if(!L||L->Materials.Contains(TEXT("EX_BrickFace0")))return;
    UMaterialInterface* Wall=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorBrick/M_Brick.M_Brick"));
    UMaterialInterface* Face=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorBrick/M_BrickFace.M_BrickFace"));
    if(!Wall||!Face){UE_LOG(LogTemp,Error,TEXT("HOUSE_BRICK missing saved masonry materials"));return;}
    UMaterialInstanceDynamic* Main=UMaterialInstanceDynamic::Create(Wall,L);
    L->Materials.Add(TEXT("EX_Brick"),Main);
    UMaterialInstanceDynamic* Chimney=UMaterialInstanceDynamic::Create(Wall,L);
    Chimney->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1.04f,.94f,.90f));
    L->Materials.Add(TEXT("EX_BrickRed"),Chimney);
    const FLinearColor Tints[]={FLinearColor(.82f,.82f,.78f),FLinearColor(1.03f,.99f,.91f),
        FLinearColor(.94f,.84f,.78f),FLinearColor(1.08f,1.02f,.93f),FLinearColor(.88f,.89f,.87f)};
    for(int32 K=0;K<5;++K)
    {
        UMaterialInstanceDynamic* M=UMaterialInstanceDynamic::Create(Face,L);
        M->SetVectorParameterValue(TEXT("Tint"),Tints[K]);
        L->Materials.Add(*FString::Printf(TEXT("EX_BrickFace%d"),K),M);
    }
    HallMakePlain(L,TEXT("EX_BrickMortar"),FLinearColor(.386f,.347f,.249f),.96f);
    UE_LOG(LogTemp,Display,TEXT("HOUSE_BRICK saved PBR relief ready; 24x8cm bond, recessed mortar, five header palettes"));
}

void Header(FHallMesh& M,const FVector& Left,const FVector& Right,const FVector& N,float H)
{
    const float Width=(Right-Left).Size();
    if(Width<4||H<6)return;
    const FVector U=(Right-Left)/Width,V=FVector::UpVector*H;
    M.Quad(TEXT("EX_BrickMortar"),Left+N*.04f,Right+N*.04f,Right+V+N*.04f,Left+V+N*.04f,N,FHallUV::World(100));
    const int32 Count=FMath::Max(1,FMath::RoundToInt(Width/8.f));
    const float Step=Width/Count;
    for(int32 K=0;K<Count;++K)
    {
        const FVector A=Left+U*(K*Step+.48f)+FVector(0,0,.45f);
        const FVector B=Left+U*((K+1)*Step-.48f)+FVector(0,0,.45f);
        Brick(M,A,B,B+FVector(0,0,H-.9f),A+FVector(0,0,H-.9f),N);
    }
}

void Arch(FHallMesh& M,float CX,float Z,float R,float Y,float Sign,float Top)
{
    const FVector N(0,Sign,0);
    // Bricks follow the existing 14-sided opening exactly. The shallow band never fills the opening.
    const int32 Steps=FMath::CeilToInt(PI*R/8.f);
    auto Inner=[&](float Angle)
    {
        const float Step=Angle/PI*14.f;
        const int32 S=FMath::Min(13,FMath::FloorToInt(Step));
        const float T=FMath::Clamp(Step-S,0.f,1.f);
        const FVector A(CX+R*FMath::Cos(S*PI/14),Y,Z+R*FMath::Sin(S*PI/14));
        const FVector B(CX+R*FMath::Cos((S+1)*PI/14),Y,Z+R*FMath::Sin((S+1)*PI/14));
        return FMath::Lerp(A,B,T);
    };
    auto Outer=[&](float Angle)
    {
        return FVector(CX+(R+21.f)*FMath::Cos(Angle),Y,FMath::Min(Top-.05f,Z+(R+21.f)*FMath::Sin(Angle)));
    };
    for(int32 K=0;K<Steps;++K)
    {
        const float A=PI*K/Steps,B=PI*(K+1)/Steps;
        M.Quad(TEXT("EX_BrickMortar"),Inner(A)+N*.04f,Inner(B)+N*.04f,Outer(B)+N*.04f,Outer(A)+N*.04f,N,FHallUV::World(100));
        const float Gap=.48f/R;
        const float T0=A+Gap,T1=B-Gap;
        const FVector IA=Inner(T0),IB=Inner(T1);
        const FVector OA=Outer(T0),OB=Outer(T1);
        Brick(M,IA+(OA-IA).GetSafeNormal()*.42f,IB+(OB-IB).GetSafeNormal()*.42f,
            OB-(OB-IB).GetSafeNormal()*.42f,OA-(OA-IA).GetSafeNormal()*.42f,N);
    }
}
}
