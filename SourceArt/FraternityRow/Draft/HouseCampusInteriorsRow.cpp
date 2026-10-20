// Candidate source only. Append RowDefs after GenericDefs in the campus registry.
// World centimetres; the permanent Row shell owns grade, exterior walls and access steps.
#include "HouseCampusInteriorsRooms.h"
#include "HouseFraternityRow.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallPaint.h"
#include "Engine/Texture2D.h"

namespace HouseCampusInteriorsImpl
{
namespace Row
{
using EKit = HouseCampusInteriors::EKit;
using FHouse = HouseFraternityRow::FHouse;
constexpr float Finish = .35f, DoorH = 213.f, OuterDoorH = 228.f;

FName Key(int32 I, const TCHAR* Suffix)
{ return FName(*FString::Printf(TEXT("CI_Row%d%s"), I + 1, Suffix)); }
FName SignKey(int32 I)
{ return FName(*FString::Printf(TEXT("CI_GenR%dSign"), I + 1)); }

struct FPlan
{
    const FHouse& H;
    float W, D, Z, Story, C, LHall, RHall, LW, RW, Y1, Y2;
    FBox2D Rooms[5], Stairs, UpperLanding;
    float DoorY[5];
    int32 HalfRisers;
    float RunEnd, ReturnEnd;
    FPlan(int32 I) : H(HouseFraternityRow::Houses()[I])
    {
        W = H.Width(); D = H.Depth(); Z = H.Floor + Finish; Story = I < 3 ? 390.f : 330.f;
        C = H.X(); LHall = C - 110.f; RHall = C + 110.f;
        // 220 clear, rather than 220 between the centres of 15 cm partitions.
        // Shared PartitionDoors adds a 3 cm wainscot cap on each face.
        LW = LHall - PartT * .5f - 3.f; RW = RHall + PartT * .5f + 3.f;
        Y1 = H.F + D * .47f; Y2 = H.F + D * .76f;
        Rooms[0] = Rect(H.L + 33.f, LW - PartT * .5f, H.F + 33.f, Y1 - PartT * .5f);
        Rooms[1] = Rect(RW + PartT * .5f, H.R - 33.f, H.F + 33.f, Y1 - PartT * .5f);
        Rooms[2] = Rect(H.L + 33.f, LW - PartT * .5f, Y1 + PartT * .5f, Y2 - PartT * .5f);
        Rooms[3] = Rect(RW + PartT * .5f, H.R - 33.f, Y1 + PartT * .5f, H.B - 33.f);
        Rooms[4] = Rect(H.L + 33.f, LW - PartT * .5f, Y2 + PartT * .5f, H.B - 33.f);
        DoorY[0] = Y1 - 110.f; DoorY[1] = H.F + D * .47f * .42f;
        DoorY[2] = (Y1 + Y2) * .5f; DoorY[3] = H.F + D * .68f;
        DoorY[4] = (Y2 + H.B) * .5f;
        Stairs = Rect(H.L + FMath::Max(50.f, W * .5f - 410.f), LHall - 25.f,
            H.F + 80.f, H.F + FMath::Min(D * .47f - 210.f, 600.f));
        HalfRisers = I < 3 ? 12 : 10;
        ReturnEnd = Stairs.Min.Y + 110.f;
        RunEnd = ReturnEnd + (HalfRisers - 1) * 28.f;
        // The top landing is ABOVE the room, not another obstacle on its ground floor.
        UpperLanding = Rect(Stairs.Max.X - 110.f, Stairs.Max.X, RunEnd, RunEnd + 110.f);
    }
    FVector V(float X, float Y, float ZOff = 0.f) const { return FVector(H.L + X, H.F + Y, Z + ZOff); }
};

bool Contains(const FBox2D& R, const FVector2D& A, const FVector2D& B)
{ return A.X >= R.Min.X && A.Y >= R.Min.Y && B.X <= R.Max.X && B.Y <= R.Max.Y; }

// Matches HouseFraternityRow::GroundWindows: aperture half-width 79, frame half-width 93.
// Keep 8cm outside the actual frame for mounted joinery, rather than hiding glazed openings.
constexpr float WindowMountHalf=101.f;
TArray<FVector2D> RearWindowExclusions(const FPlan& P)
{
    const bool Old=P.H.Index<3;const int32 Count=Old?5:3;
    const float End=Old?230.f:200.f;
    TArray<FVector2D> Out;
    for(int32 N=0;N<Count;++N)if(N!=Count/2)
    {
        float X=FMath::Lerp(P.H.L+End,P.H.R-End,float(N)/(Count-1));
        if(P.H.Index==4&&N==0)X=P.H.L+280;
        Out.Add(FVector2D(X-WindowMountHalf,X+WindowMountHalf));
    }
    return Out;
}

FVector2D ChapterDisplayPier(const FPlan& P)
{
    // First opaque pier between the actual east-wall casements; all five clear widths
    // exceed the 245cm display group (155cm case, three 22cm paddles, three 8cm gaps).
    const bool Old=P.H.Index<3;const float End=Old?280.f:250.f;
    const float First=P.H.F+End,Step=(P.D-2*End)/(Old?3.f:2.f);
    return FVector2D(First+WindowMountHalf,First+Step-WindowMountHalf);
}

void WindowAwareShelves(FHallMesh& S,FHallMesh& C,const FPlan& P,const FBox2D& Kitchen)
{
    const float X0=Kitchen.Min.X+35,X1=Kitchen.Max.X-150,Wall=P.H.B-31.5f;
    const TArray<FVector2D> Gaps=RearWindowExclusions(P);
    for(float Z:{155.f,198.f})
    {
        auto Board=[&](float A,float B)
        {
            if(B<=A+1.f)return;
            Box(S,TEXT("CI_OakDark"),FVector(A,Wall-32,P.Z+Z),FVector(B,Wall,P.Z+Z+3));
            // Ordinary steel L brackets attach each useful shelf section to its opaque
            // backing. Even the longest board has support intervals no greater than85cm.
            const int32 Supports=FMath::Max(2,FMath::CeilToInt((B-A-40.f)/85.f)+1);
            for(int32 N=0;N<Supports;++N)
            {
                const float X=FMath::Lerp(A+20.f,B-20.f,float(N)/(Supports-1));
                Box(C,TEXT("CI_Steel"),FVector(X-1.5f,Wall-1,P.Z+Z-24),FVector(X+1.5f,Wall,P.Z+Z));
                Box(C,TEXT("CI_Steel"),FVector(X-1.5f,Wall-27,P.Z+Z-2),FVector(X+1.5f,Wall,P.Z+Z));
            }
        };
        float At=X0;
        for(const FVector2D& Gap:Gaps)
        {
            if(Gap.Y<=At||Gap.X>=X1)continue;
            Board(At,FMath::Min(Gap.X,X1));
            At=FMath::Max(At,Gap.Y);
            if(At>=X1)break;
        }
        Board(At,X1);
    }
}

// The existing kit has heterogeneous real sizes. Reject a placement that would leave its reserved footprint.
bool FitKit(const FKitter& K, EKit Piece, const FVector2D& At, float Z, float Yaw,
    const FBox2D& Reservation, bool bProp = false)
{
    if (!K.Sub || !K.Out)
    {
        UE_LOG(LogTemp,Warning,TEXT("FRAT_ROW_KIT_REJECT kit=%d reason=missingKitter subsystem=%d output=%d target=(%.2f,%.2f,%.2f) yaw=%.2f available=[%.2f,%.2f,%.2f,%.2f] prop=%d"),
            int32(Piece),int(K.Sub!=nullptr),int(K.Out!=nullptr),At.X,At.Y,Z,Yaw,Reservation.Min.X,Reservation.Max.X,Reservation.Min.Y,Reservation.Max.Y,int(bProp));
        return false;
    }
    const FBox B = K.Sub->KitBounds(Piece);
    if (!B.IsValid)
    {
        UE_LOG(LogTemp,Warning,TEXT("FRAT_ROW_KIT_REJECT kit=%d reason=invalidAssetBounds realHalf=unavailable target=(%.2f,%.2f,%.2f) yaw=%.2f available=[%.2f,%.2f,%.2f,%.2f] prop=%d"),
            int32(Piece),At.X,At.Y,Z,Yaw,Reservation.Min.X,Reservation.Max.X,Reservation.Min.Y,Reservation.Max.Y,int(bProp));
        return false;
    }
    const FQuat Q = FRotator(0, Yaw + KitDef(Piece).FrontYaw, 0).Quaternion();
    const FVector E = B.GetExtent();
    const FVector RX = Q.RotateVector(FVector(E.X, 0, 0)), RY = Q.RotateVector(FVector(0, E.Y, 0));
    const FVector2D Half(FMath::Abs(RX.X) + FMath::Abs(RY.X), FMath::Abs(RX.Y) + FMath::Abs(RY.Y));
    if (!Contains(Reservation, At - Half, At + Half))
    {
        UE_LOG(LogTemp,Warning,TEXT("FRAT_ROW_KIT_REJECT kit=%d reason=footprint realHalf=(%.2f,%.2f,%.2f) rotatedHalf=(%.2f,%.2f) target=(%.2f,%.2f,%.2f) yaw=%.2f actual=[%.2f,%.2f,%.2f,%.2f] available=[%.2f,%.2f,%.2f,%.2f] prop=%d"),
            int32(Piece),E.X,E.Y,E.Z,Half.X,Half.Y,At.X,At.Y,Z,Yaw,At.X-Half.X,At.X+Half.X,At.Y-Half.Y,At.Y+Half.Y,
            Reservation.Min.X,Reservation.Max.X,Reservation.Min.Y,Reservation.Max.Y,int(bProp));
        return false;
    }
    K.Floor(Piece, At, Z, Yaw, bProp);
    return true;
}

void Seat(FHallMesh& Solid, const FKitter& K, const FVector& C, float Yaw,
    const FBox2D& Reservation, bool bProp = true)
{
    if (!FitKit(K, EKit::SpindleChair, FVector2D(C.X, C.Y), C.Z, Yaw, Reservation, bProp))
        Chair(Solid, C, FRotator(0, Yaw, 0).Vector());
}

void Sofa(FHallMesh& Solid, const FKitter& K, const FVector& C, float Yaw,
    const FBox2D& Reservation, bool bLeather)
{
    if (FitKit(K, bLeather ? EKit::LeatherSofa : EKit::Loveseat, FVector2D(C.X, C.Y), C.Z, Yaw, Reservation)) return;
    const FVector F = FRotator(0, Yaw, 0).Vector(), R(-F.Y, F.X, 0);
    const FTransform T(FRotator(0, Yaw, 0), C);
    Solid.Box(TEXT("CI_RowUpholstery"), FTransform(T.GetRotation(),T.TransformPosition(FVector(-4,0,27))), FVector(43, 80, 27), UVPlain());
    Solid.Box(TEXT("CI_RowUpholstery"), FTransform(T.GetRotation(),T.TransformPosition(FVector(-40,0,59))), FVector(9, 80, 30), UVPlain());
    for (float S : {-1.f, 1.f})
        Solid.Box(TEXT("CI_OakDark"), FTransform(FRotator(0, Yaw, 0), C + R * S * 83.f + FVector(0, 0, 42)), FVector(44, 7, 25), UVPlain());
}

void WoodFloor(FHallMesh& Clutter, const FBox2D& R, float Z, FName Mat, int32 I)
{
    Box(Clutter, Mat, FVector(R.Min, Z - .12f), FVector(R.Max, Z));
    // Sparse joints, not a separate mesh actor or material per plank.
    for (float X = R.Min.X + 16.f; X < R.Max.X; X += 16.f)
        Box(Clutter, TEXT("CI_RowJoint"), FVector(X, R.Min.Y, Z + .01f), FVector(X + .12f, R.Max.Y, Z + .04f));
    for (float X = R.Min.X; X < R.Max.X; X += 128.f)
        for (float Y = R.Min.Y + ((int32((X - R.Min.X) / 128.f) + I) % 3) * 80.f; Y < R.Max.Y; Y += 240.f)
            Box(Clutter, TEXT("CI_RowJoint"), FVector(X, Y, Z + .01f), FVector(FMath::Min(X + 128.f, R.Max.X), Y + .12f, Z + .04f));
}

void CornerSector(FHallMesh& M,FName Mat,const FPlan& P,float Radius,float Z,bool Up)
{
    const FVector Center(P.H.L+150,P.H.F+150,Z), N=Up?FVector::UpVector:-FVector::UpVector;
    for(int32 J=0;J<4;++J)
    {
        const float A=PI+J*PI/8, B=PI+(J+1)*PI/8;
        const FVector V0=Center+FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,0);
        const FVector V1=Center+FVector(FMath::Cos(B)*Radius,FMath::Sin(B)*Radius,0);
        FHallSection& S=M.Sec(Mat); const int32 Base=S.V.Num();
        for(const FVector V : {Center,V0,V1})
        {
            S.V.Add(V);S.N.Add(N);S.UV.Add(FVector2D(V.X/100,V.Y/100));S.T.Add(FProcMeshTangent(FVector::ForwardVector,false));
        }
        if(FVector::DotProduct(FVector::CrossProduct(V0-Center,V1-Center),N)<0)S.I.Append({Base,Base+1,Base+2});
        else S.I.Append({Base,Base+2,Base+1});
    }
}

void CompositeLook(UHouseBlackHallSubsystem* Look, int32 I, int32 Year)
{
    FHallImage Img; const FLinearColor Paper(.76f, .66f, .46f), Ink(.08f, .06f, .04f);
    Img.Init(512, 384, Paper);
    Img.Rect(FVector2D(7, 7), FVector2D(505, 377), Ink);
    Img.Rect(FVector2D(13, 13), FVector2D(499, 371), Paper);
    FHallTextStyle TS; TS.Height = 23; TS.Stroke = 2.4f;
    const FString Title = FString::Printf(TEXT("HOUSE %d  -  %d"), I + 1, Year);
    HallDrawText(Img, Title, FVector2D((512 - HallTextWidth(Title, TS)) * .5f, 49), TS, Ink);
    // Original drawn silhouettes. No source photographs, names or real organizational artwork.
    for (int32 Y = 0; Y < 3; ++Y) for (int32 X = 0; X < 7; ++X)
    {
        const FVector2D C(46.f + X * 70.f, 97.f + Y * 88.f);
        Img.Rect(C - FVector2D(23, 27), C + FVector2D(23, 35), FLinearColor(.38f, .34f, .27f));
        const float Shade = .19f + ((X * 3 + Y + I) % 5) * .025f;
        Img.Ellipse(C + FVector2D(0, -7), FVector2D(8, 11), 0, FLinearColor(.56f, .45f, .32f));
        Img.Ellipse(C + FVector2D(0, 22), FVector2D(18, 19), 0, FLinearColor(Shade, Shade * .92f, Shade * .82f));
        Img.Rect(C + FVector2D(-20, 39), C + FVector2D(20, 41), Ink);
    }
    const FName Mat = Key(I, Year == 1986 ? TEXT("Composite86") : TEXT("Composite79"));
    UTexture2D* Tex = HallMakeTexture(Img, true, false, *Mat.ToString(), false);
    if (Tex) Look->Textures.Add(Mat, Tex);
    HallMakeTextured(Look, Mat, Tex, nullptr, .67f);
}

void Look(UHouseBlackHallSubsystem* L)
{
    if (!L || L->Materials.Contains(Key(0, TEXT("Plaster")))) return;
    FixturesLook(L);
    const FLinearColor Paint[] = {{.36f,.32f,.24f},{.62f,.62f,.48f},{.50f,.36f,.29f},{.67f,.61f,.46f},{.43f,.33f,.31f}};
    const FLinearColor Carpet[] = {{.14f,.17f,.12f},{.18f,.23f,.19f},{.25f,.09f,.06f},{.19f,.14f,.08f},{.27f,.12f,.12f}};
    const TCHAR* Identities[] = {TEXT("OMEGA XI LAMBDA ETA"), TEXT("PSI ETA XI RHO"), TEXT("ETA XI OMEGA RHO"), TEXT("LAMBDA OMEGA ETA XI"), TEXT("RHO XI ETA PSI")};
    for (int32 I = 0; I < 5; ++I)
    {
        PaintNoise(L, Key(I,TEXT("Plaster")), Paint[I], .026f, .9f, 920 + I, I == 2 ? 55 : 12, FLinearColor(.19f,.17f,.13f));
        PaintNoise(L, Key(I,TEXT("Carpet")), Carpet[I], .09f, .97f, 940 + I, I == 2 ? 90 : 15, FLinearColor(.11f,.09f,.07f));
        PaintNoise(L, Key(I,TEXT("Wood")), I == 3 ? FLinearColor(.22f,.12f,.045f) : I == 4 ? FLinearColor(.16f,.075f,.026f) : FLinearColor(.31f,.19f,.085f), .025f,.73f,960+I,25,FLinearColor(.17f,.095f,.035f));
        PaintText(L, SignKey(I), 512, 256, FLinearColor(.085f,.06f,.027f), FLinearColor(.78f,.66f,.35f),
            {{Identities[I],.13f},{TEXT("CHAPTER HOUSE"),.11f},{TEXT("OCTOBER 1986"),.075f}}, true);
        PaintText(L, Key(I,TEXT("Notice")),512,384,FLinearColor(.78f,.73f,.57f),FLinearColor(.14f,.09f,.05f),
            {{TEXT("HOUSE NOTICES"),.10f},{TEXT("HOMECOMING - OCT 24"),.07f},{TEXT("KITCHEN DUTY - 9 AM"),.07f},{TEXT("RETURN BORROWED CHAIRS"),.065f}},true);
        CompositeLook(L,I,1979); CompositeLook(L,I,1986);
    }
    PaintNoise(L,TEXT("CI_RowLino"),FLinearColor(.48f,.47f,.32f),.06f,.83f,973,75,FLinearColor(.25f,.24f,.16f));
    PaintNoise(L,TEXT("CI_RowTile"),FLinearColor(.69f,.69f,.58f),.03f,.65f,974,30,FLinearColor(.40f,.41f,.34f));
    PaintNoise(L,TEXT("CI_RowSlate"),FLinearColor(.16f,.18f,.17f),.07f,.88f,975);
    HallMakePlain(L,TEXT("CI_RowJoint"),FLinearColor(.055f,.037f,.02f),.94f);
    HallMakePlain(L,TEXT("CI_RowPorcelain"),FLinearColor(.72f,.71f,.59f),.22f);
    HallMakePlain(L,TEXT("CI_RowEnamel"),FLinearColor(.57f,.59f,.46f),.33f);
    HallMakePlain(L,TEXT("CI_RowMirror"),FLinearColor(.31f,.38f,.39f),.12f,.72f);
    HallMakePlain(L,TEXT("CI_RowUpholstery"),FLinearColor(.19f,.12f,.073f),.96f);
    HallMakePlain(L,TEXT("CI_RowSpill"),FLinearColor(.055f,.035f,.014f),.33f);
    PaintNoise(L,TEXT("CI_RowQuietCurtain"),FLinearColor(.10f,.075f,.055f),.025f,.99f,983);
    HallMakePlain(L,TEXT("CI_RowDead_CI_CrtScreen"),FLinearColor(.023f,.032f,.031f),.26f);
    HallMakePlain(L,TEXT("CI_RowDead_CI_DomeLight"),FLinearColor(.49f,.44f,.32f),.75f);
    HallMakePlain(L,TEXT("CI_RowDead_CI_Troffer"),FLinearColor(.38f,.40f,.33f),.73f);
    HallMakePlain(L,TEXT("CI_RowDead_CI_HoodLight"),FLinearColor(.42f,.39f,.30f),.72f);
    PaintText(L,TEXT("CI_RowUpper"),512,256,FLinearColor(.18f,.115f,.05f),FLinearColor(.78f,.67f,.42f),
        {{TEXT("RESIDENT ROOMS"),.13f},{TEXT("PRIVATE - KEEP CLOSED"),.095f}},true);
    PaintText(L,TEXT("CI_RowWC"),256,128,FLinearColor(.18f,.14f,.07f),FLinearColor(.8f,.75f,.54f),{{TEXT("SHARED WC"),.18f}},true);
    PaintText(L,TEXT("CI_RowKitchen"),256,128,FLinearColor(.18f,.14f,.07f),FLinearColor(.8f,.75f,.54f),{{TEXT("KITCHEN"),.18f}},true);
    PaintText(L,TEXT("CI_RowChapter"),256,128,FLinearColor(.18f,.14f,.07f),FLinearColor(.8f,.75f,.54f),{{TEXT("CHAPTER ROOM"),.14f}},true);
}

void Framed(FHallMesh& C, FName Mat, const FVector& At, const FVector& Out, float W, float H)
{
    const FVector N = Out.GetSafeNormal2D(), R(-N.Y,N.X,0);
    C.Box(TEXT("CI_OakDark"),FTransform(FRotationMatrix::MakeFromXZ(R,FVector::UpVector).Rotator(),At),FVector(W*.5f+3,2.5f,H*.5f+3),UVPlain());
    WallPlaque(C,Mat,At+N*3.f,N,W,H);
}

void Paddle(FHallMesh& C,const FVector& At,const FVector& Out,int32 Variant)
{
    const FVector R(-Out.Y,Out.X,0); const FVector U = FVector::UpVector;
    const FTransform T(FRotationMatrix::MakeFromXZ(R,U).Rotator(),At);
    C.Box(TEXT("CI_Oak"),T,FVector(11.f,1.4f,21.f),UVPlain());
    C.Box(TEXT("CI_OakDark"),FTransform(T.GetRotation(),At+U*31.f),FVector(4.f,1.4f,12.f),UVPlain());
    for(int32 K=0;K<Variant+2;++K)
        C.Cylinder(TEXT("CI_Brass"),At+Out*1.7f+R*(-7.f+K*4.f)-U*6.f,At+Out*1.7f+R*(-7.f+K*4.f)+U*6.f, .8f,5,UVPlain());
}

void Pendant(FHallMesh& C,FHallMesh& G,const FVector& At)
{
    C.Cylinder(TEXT("CI_Brass"),At,At-FVector(0,0,32),1.2f,6,UVPlain());
    G.Dome(TEXT("CI_DomeLight"),At-FVector(0,0,32),-FVector::UpVector,18,9,12);
}

void Fireplace(FHallMesh& S,FHallMesh& C,const FVector& At,const FVector& Facing,float Width)
{
    const FVector F = Facing.GetSafeNormal2D(), R(-F.Y,F.X,0);
    auto B=[&](FName M,float X0,float X1,float Y0,float Y1,float Z0,float Z1)
    {
        const FVector Mid=At+F*((X0+X1)*.5f)+R*((Y0+Y1)*.5f)+FVector(0,0,(Z0+Z1)*.5f);
        S.Box(M,FTransform(FRotationMatrix::MakeFromXZ(F,FVector::UpVector).Rotator(),Mid),FVector((X1-X0)*.5f,(Y1-Y0)*.5f,(Z1-Z0)*.5f),UVPlain());
    };
    B(TEXT("CI_RowSlate"),0,70,-Width*.5f,Width*.5f,0,9);
    B(TEXT("CI_OakDark"),0,38,-Width*.5f,-Width*.5f+25,9,130);
    B(TEXT("CI_OakDark"),0,38,Width*.5f-25,Width*.5f,9,130);
    B(TEXT("CI_Oak"),0,55,-Width*.5f-8,Width*.5f+8,125,140);
    B(TEXT("CI_RowJoint"),0,5,-Width*.5f+25,Width*.5f-25,9,125);
    for(int32 I=0;I<3;++I)
    {
        const FVector P=At+F*22+R*(I-1)*17+FVector(0,0,20+I*4);
        C.Cylinder(TEXT("CI_OakDark"),P-R*15,P+R*15,5,7,UVPlain());
    }
}

void Stairs(FHallMesh& S,FHallMesh& C,const FPlan& P)
{
    const float XL=P.Stairs.Min.X, XR=P.Stairs.Max.X, South=P.Stairs.Min.Y, Turn=P.ReturnEnd;
    const float Rise=P.Story/(P.HalfRisers*2.f), Mid=P.Z+P.Story*.5f, Top=P.Z+P.Story;
    const float XA[2]={XL,XR-110.f}, XB[2]={XL+110.f,XR};
    // First flight starts north and travels south. The return flight goes north.
    for(int32 F=0;F<2;++F) for(int32 K=0;K<P.HalfRisers-1;++K)
    {
        const float A=F==0?P.RunEnd-(K+1)*28.f:Turn+K*28.f;
        const float Z0=F==0?P.Z:Mid, Z1=Z0+(K+1)*Rise;
        Box(S,Key(P.H.Index,TEXT("Wood")),FVector(XA[F],A,P.Z-10),FVector(XB[F],A+28,Z1));
        const float Nose=F==0?A+26:A;
        Box(C,TEXT("CI_OakDark"),FVector(XA[F],Nose,Z1),FVector(XB[F],Nose+2,Z1+.2f));
        // Solid visible guards follow the tread; clear flight width stays110cm.
        for(float X : {XA[F]-7.f,XB[F]+1.f})
        {
            Box(S,TEXT("CI_OakDark"),FVector(X,A,Z1),FVector(X+6,A+28,Z1+96));
            Box(C,TEXT("CI_Brass"),FVector(X-.5f,A,Z1+96),FVector(X+6.5f,A+28,Z1+100));
        }
    }
    Box(S,Key(P.H.Index,TEXT("Wood")),FVector(XL,South,P.Z-10),FVector(XR,Turn,Mid));
    // Tenth/twelfth riser is the return landing; final riser is the upper floor.
    Box(S,TEXT("CI_StairTread"),FVector(P.UpperLanding.Min,Top-18),FVector(P.UpperLanding.Max,Top));
    Balustrade(S,FVector2D(XL,South),FVector2D(XR,South),Mid);
    Balustrade(S,FVector2D(XL,South),FVector2D(XL,Turn),Mid);
    Balustrade(S,FVector2D(XR,South),FVector2D(XR,Turn),Mid);
    // The remaining upper volume is closed by drawn walls, not invisible collision.
    const float UL=P.UpperLanding.Min.X, UR=P.UpperLanding.Max.X, Y0=P.UpperLanding.Min.Y,Y1=P.UpperLanding.Max.Y;
    for(float X : {UL-PartT*.5f,UR+PartT*.5f})
        Box(S,TEXT("CI_OakDark"),FVector(X-PartT*.5f,Y0,Top),FVector(X+PartT*.5f,Y1+PartT,Top+240));
    Partition(S,TEXT("CI_OakDark"),false,Y1+PartT*.5f,UL-PartT,UR+PartT,Top,Top+240,{{UL+7.f,UR-7.f}},DoorH);
    Box(S,TEXT("CI_Plaster"),FVector(UL-PartT,Y0,Top+240),FVector(UR+PartT,Y1+PartT,Top+252));
    // Guard the ceiling's other stairwell edges on its upper side.
    Balustrade(S,FVector2D(XL-8,South-8),FVector2D(XR+8,South-8),Top);
    Balustrade(S,FVector2D(XL-8,South-8),FVector2D(XL-8,P.RunEnd+8),Top);
    Framed(C,TEXT("CI_RowUpper"),FVector((UL+UR)*.5f,Y1-.5f,Top+159),FVector(0,-1,0),85,28);
}

// Retained timber casing has a real core, side boards and a bottom board. Every part uses
// the same stair/landing cutouts as the ceiling; no decorative board can bridge that void.
void CasedBeam(FHallMesh& C,const FBox2D& Core,bool AlongX,float Ceiling,const TArray<FBox2D>& Holes)
{
    auto Board=[&](const FBox2D& R,float Low,float High,FName Mat)
    {
        // SubtractPlan expects intersecting holes; an off-board hole can otherwise yield
        // an inverted clamped interval and extend a narrow board outside its reservation.
        TArray<FBox2D> Relevant;
        for(const FBox2D& H:Holes)
            if(H.Min.X<R.Max.X&&H.Max.X>R.Min.X&&H.Min.Y<R.Max.Y&&H.Max.Y>R.Min.Y)Relevant.Add(H);
        SubtractPlan(R,Relevant,[&](const FBox2D& A){Box(C,Mat,FVector(A.Min,Low),FVector(A.Max,High));});
    };
    Board(Core,Ceiling-16,Ceiling,TEXT("CI_OakDark"));
    if(AlongX)
    {
        Board(Rect(Core.Min.X,Core.Max.X,Core.Min.Y-3,Core.Min.Y),Ceiling-22,Ceiling,TEXT("CI_Oak"));
        Board(Rect(Core.Min.X,Core.Max.X,Core.Max.Y,Core.Max.Y+3),Ceiling-22,Ceiling,TEXT("CI_Oak"));
        Board(Rect(Core.Min.X,Core.Max.X,Core.Min.Y-3,Core.Max.Y+3),Ceiling-25,Ceiling-22,TEXT("CI_OakDark"));
    }
    else
    {
        Board(Rect(Core.Min.X-3,Core.Min.X,Core.Min.Y,Core.Max.Y),Ceiling-22,Ceiling,TEXT("CI_Oak"));
        Board(Rect(Core.Max.X,Core.Max.X+3,Core.Min.Y,Core.Max.Y),Ceiling-22,Ceiling,TEXT("CI_Oak"));
        Board(Rect(Core.Min.X-3,Core.Max.X+3,Core.Min.Y,Core.Max.Y),Ceiling-25,Ceiling-22,TEXT("CI_OakDark"));
    }
}

void TudorCorbel(FHallMesh& C,const FVector& Wall,float Inward,float Ceiling)
{
    // These sit on solid wall piers between casements, not in window openings.
    const float X=Wall.X,Y=Wall.Y;
    Box(C,TEXT("CI_OakDark"),FVector(FMath::Min(X,X+Inward*7),Y-15,Ceiling-82),
        FVector(FMath::Max(X,X+Inward*7),Y+15,Ceiling));
    Box(C,TEXT("CI_Oak"),FVector(FMath::Min(X,X+Inward*58),Y-15,Ceiling-25),
        FVector(FMath::Max(X,X+Inward*58),Y+15,Ceiling-17));
    const FVector A(X+Inward*7,Y,Ceiling-78),B(X+Inward*49,Y,Ceiling-25);
    const FVector Axis=(B-A).GetSafeNormal();
    C.Box(TEXT("CI_OakDark"),FTransform(FRotationMatrix::MakeFromXZ(Axis,FVector::UpVector).Rotator(),(A+B)*.5f),
        FVector(FVector::Dist(A,B)*.5f,7,7),UVPlain());
    for(float T:{.2f,.76f})
    {
        const FVector Peg=FMath::Lerp(A,B,T);
        C.Cylinder(TEXT("CI_Oak"),Peg-FVector(0,7.4f,0),Peg+FVector(0,7.4f,0),1.4f,8,UVPlain());
    }
}

void RetainedJoinery(FHallMesh& C,const FPlan& P,int32 I,float Ceiling,const TArray<FBox2D>& CeilingHoles)
{
    if(I==3)
    {
        // Oak beam ends and knee braces sit between the side casements at local Y250/700.
        // Their lowest point is over 227cm; the ground walking route remains clear.
        for(float Y:{P.H.F+390.f,P.H.F+590.f})
        {
            CasedBeam(C,Rect(P.H.L+33,P.H.R-33,Y,Y+16),true,Ceiling,CeilingHoles);
            TudorCorbel(C,FVector(P.H.L+31.5f,Y+8,0),1,Ceiling);
            TudorCorbel(C,FVector(P.H.R-31.5f,Y+8,0),-1,Ceiling);
        }
        // The surviving low oak panelling stops below the 95cm casement sills.
        Wainscot(C,false,P.H.L+31.5f,1,P.Rooms[0].Min.Y,P.Rooms[0].Max.Y,P.Z,{});
    }
    else if(I==4)
    {
        for(float Y=P.H.F+260;Y<P.Y1;Y+=210)
            CasedBeam(C,Rect(P.H.L+33,P.H.R-33,Y,Y+16),true,Ceiling,CeilingHoles);
        // A cased-beam chapter-parlor ceiling survives the conversion to student use.
        // Longitudinal members butt into the transverse casings instead of overlapping them.
        const FBox2D& R=P.Rooms[1];
        TArray<FBox2D> Crossings;
        for(float Y=P.H.F+260;Y<P.Y1;Y+=210)
            Crossings.Add(Rect(R.Min.X-1,R.Max.X+1,Y-3,Y+19));
        for(float T:{1.f/3.f,2.f/3.f})
        {
            const float X=FMath::Lerp(R.Min.X,R.Max.X,T);
            SubtractPlan(Rect(X-8,X+8,R.Min.Y,R.Max.Y),Crossings,[&](const FBox2D& A)
            {CasedBeam(C,A,false,Ceiling,CeilingHoles);});
        }
        // Original picture rail: above 277cm window heads and below the 312cm ceiling.
        const FBox2D Rail=Rect(P.RW+PartT*.5f,P.H.R-31.5f,P.H.F+31.5f,P.Y1-PartT*.5f);
        for(int32 Tier=0;Tier<2;++Tier)
        {
            const float Depth=Tier==0?1.5f:3.5f;
            const float Low=Ceiling-(Tier==0?28.f:24.f),High=Ceiling-(Tier==0?24.f:22.f);
            Box(C,TEXT("CI_Oak"),FVector(Rail.Min.X,Rail.Min.Y,Low),FVector(Rail.Max.X,Rail.Min.Y+Depth,High));
            Box(C,TEXT("CI_Oak"),FVector(Rail.Min.X,Rail.Max.Y-Depth,Low),FVector(Rail.Max.X,Rail.Max.Y,High));
            Box(C,TEXT("CI_Oak"),FVector(Rail.Min.X,Rail.Min.Y+Depth,Low),FVector(Rail.Min.X+Depth,Rail.Max.Y-Depth,High));
            Box(C,TEXT("CI_Oak"),FVector(Rail.Max.X-Depth,Rail.Min.Y+Depth,Low),FVector(Rail.Max.X,Rail.Max.Y-Depth,High));
        }
    }
}

void Shell(FHallMesh& S,FHallMesh& C,FHallMesh& G,const FPlan& P,int32 I)
{
    // Same openings as permanent walls: windows remain visible and transparent glazing owns collision.
    HouseFraternityRow::Walls(C,P.H,Key(I,TEXT("Plaster")),1.5f,30.f);
    const FBox2D Inner=Rect(P.H.L+31.5f,P.H.R-31.5f,P.H.F+31.5f,P.H.B-31.5f);
    const FBox2D Corner=Rect(P.H.L+31.5f,P.H.L+150,P.H.F+31.5f,P.H.F+150);
    // A continuous owned finish also makes floor-anchor tests independent of the exterior actor's identity.
    const TArray<FBox2D> GroundHoles=I==4?TArray<FBox2D>{Corner}:TArray<FBox2D>();
    SubtractPlan(Inner,GroundHoles,[&](const FBox2D& R){Box(S,Key(I,TEXT("Wood")),FVector(R.Min,P.H.Floor-.1f),FVector(R.Max,P.Z));});
    if(I==4) CornerSector(S,Key(I,TEXT("Wood")),P,118.5f,P.Z,true);
    const FBox2D Well=Rect(P.Stairs.Min.X-8,P.Stairs.Max.X+8,P.Stairs.Min.Y-8,P.RunEnd+1);
    TArray<FBox2D> CeilingHoles={Well,P.UpperLanding}; if(I==4)CeilingHoles.Add(Corner);
    FloorSlab(S,Inner,P.Z+P.Story,18.f,CeilingHoles,Key(I,TEXT("Wood")),Key(I,TEXT("Plaster")),UVPlain());
    if(I==4)
    {
        CornerSector(S,Key(I,TEXT("Wood")),P,118.5f,P.Z+P.Story,true);
        CornerSector(S,Key(I,TEXT("Plaster")),P,118.5f,P.Z+P.Story-18,false);
    }
    const TArray<FVector2D> LeftDoors={{P.DoorY[0]-70,P.DoorY[0]+70},{P.DoorY[2]-70,P.DoorY[2]+70},{P.DoorY[4]-70,P.DoorY[4]+70}};
    const TArray<FVector2D> RightDoors={{P.DoorY[1]-70,P.DoorY[1]+70},{P.DoorY[3]-70,P.DoorY[3]+70}};
    const float Ceiling=P.Z+P.Story-18.f;
    PartitionDoors(S,Key(I,TEXT("Plaster")),true,P.LW,P.H.F+30,P.H.B-30,P.Z,Ceiling,LeftDoors,DoorH);
    PartitionDoors(S,Key(I,TEXT("Plaster")),true,P.RW,P.H.F+30,P.H.B-30,P.Z,Ceiling,RightDoors,DoorH);
    const TArray<FVector2D> Parlor=I==4?TArray<FVector2D>{{P.H.L+100,P.H.L+300}}:TArray<FVector2D>();
    PartitionDoors(S,Key(I,TEXT("Plaster")),false,P.Y1,P.H.L+30,P.LW,P.Z,Ceiling,Parlor,DoorH);
    Partition(S,Key(I,TEXT("Plaster")),false,P.Y2,P.H.L+30,P.LW,P.Z,Ceiling,{},DoorH);
    Partition(S,Key(I,TEXT("Plaster")),false,P.Y1,P.RW,P.H.R-30,P.Z,Ceiling,{},DoorH);
    WoodFloor(C,Rect(P.LHall,P.RHall,P.H.F+32,P.H.B-32),P.Z+.03f,Key(I,TEXT("Wood")),I);
    if(I==3) Box(C,TEXT("CI_RowSlate"),FVector(P.LHall,P.H.F+32,P.Z+.04f),FVector(P.RHall,P.H.F+220,P.Z+.09f));
    for(int32 R=0;R<5;++R)
    {
        const FName Mat=R==3?FName(TEXT("CI_RowLino")):R==4?FName(TEXT("CI_RowTile")):R==0&&I!=3&&I!=4?Key(I,TEXT("Carpet")):Key(I,TEXT("Wood"));
        if(I==4&&R==0)
        {
            SubtractPlan(P.Rooms[R],{Corner},[&](const FBox2D& A){WoodFloor(C,A,P.Z+.03f,Mat,I+R);});
            CornerSector(C,Mat,P,117,P.Z+.03f,true);
        }
        else if(Mat==Key(I,TEXT("Wood"))) WoodFloor(C,P.Rooms[R],P.Z+.03f,Mat,I+R);
        else Box(C,Mat,FVector(P.Rooms[R].Min,P.Z+.01f),FVector(P.Rooms[R].Max,P.Z+.07f));
    }
    RetainedJoinery(C,P,I,Ceiling,CeilingHoles);
    for(float Y : {P.H.F+300.f,P.H.F+P.D*.54f,P.H.B-210.f})
        Pendant(C,G,FVector(P.C,Y,Ceiling));
    const float LabelX=P.RW-PartT*.5f-.7f;
    WallPlaque(C,TEXT("CI_RowChapter"),FVector(LabelX,P.DoorY[1],P.Z+245),FVector(-1,0,0),116,22);
    WallPlaque(C,TEXT("CI_RowKitchen"),FVector(LabelX,P.DoorY[3],P.Z+245),FVector(-1,0,0),100,22);
    WallPlaque(C,TEXT("CI_RowWC"),FVector(P.LW+PartT*.5f+.7f,P.DoorY[4],P.Z+245),FVector(1,0,0),100,22);
    Framed(C,SignKey(I),FVector(P.C,P.H.F+32,P.Z+272),FVector(0,1,0),178,74);
    Framed(C,Key(I,TEXT("Composite79")),FVector(P.LW+PartT*.5f+1,P.H.F+240,P.Z+170),FVector(1,0,0),115,86);
    Framed(C,Key(I,TEXT("Composite86")),FVector(P.LW+PartT*.5f+1,P.H.F+390,P.Z+170),FVector(1,0,0),115,86);
    BulletinBoard(S,C,FVector(P.RW+PartT*.5f,P.H.B-190,P.Z+158),FVector(1,0,0),120,92,Key(I,TEXT("Notice")));
    WallClock(C,FVector(P.RW-PartT*.5f,P.H.F+P.D*.44f,P.Z+260),FVector(-1,0,0),17);
    // Coin phone is on the room face. It does not narrow the 220 cm clear hall.
    PayPhone(S,C,FVector(P.RW+PartT*.5f,P.Y1-115,P.Z+148),FVector(1,0,0));
    Stairs(S,C,P);
}

void Lounge(FHallMesh& S,FHallMesh& C,FHallMesh& G,const FKitter& K,const FPlan& P,int32 I)
{
    const FBox2D& R=P.Rooms[0];
    const float XR=P.Stairs.Min.X-42.f;
    const FBox2D Free=Rect(R.Min.X+12,XR,R.Min.Y+12,R.Max.Y-25);
    if(I==4)
    {
        // SW turret-shaped reading nook, entirely inside the approved body envelope.
        const FBox2D Alcove=Rect(P.H.L+40,P.H.L+260,P.H.F+40,P.H.F+260);
        const FBox2D Cut=Rect(P.H.L+40,P.H.L+150,P.H.F+40,P.H.F+150);
        SubtractPlan(Alcove,{Cut},[&](const FBox2D& A){Box(C,Key(I,TEXT("Carpet")),FVector(A.Min,P.Z+.08f),FVector(A.Max,P.Z+.13f));});
        CornerSector(C,Key(I,TEXT("Carpet")),P,110,P.Z+.13f,true);
        Seat(S,K,P.V(145,150),35,Alcove,false);
        Table(S,P.V(105,318),65,48,61);
        FitKit(K,EKit::FloorLamp,FVector2D(P.H.L+65,P.H.F+615),P.Z,0,Rect(P.H.L+35,P.H.L+95,P.H.F+565,P.H.F+665));
        // Preserve the real89cm aisle between sofa and stair guard for the32cm-radius capsule.
        Sofa(S,K,P.V(150,486),0,Rect(P.H.L+100,P.H.L+194,P.H.F+395,P.H.F+590),false);
        Framed(C,Key(I,TEXT("Composite79")),P.V(33,500,166),FVector(1,0,0),100,76);
    }
    else if(I==3)
    {
        Fireplace(S,C,FVector(P.H.L+190,R.Max.Y,P.Z),FVector(0,-1,0),165);
        // Native leather-sofa half width 105.75cm needs 5cm more west clearance at the stair reservation.
        Sofa(S,K,P.V(240,235),90,Rect(Free.Min.X,Free.Max.X,P.H.F+120,P.H.F+340),true);
        Table(S,P.V(175,440),90,55,48);
        Seat(S,K,P.V(120,545),90,Rect(Free.Min.X,Free.Max.X,P.H.F+475,P.Y1-20));
        FitKit(K,EKit::FloorLamp,FVector2D(P.H.L+100,P.H.F+100),P.Z,0,Rect(P.H.L+50,P.H.L+150,P.H.F+50,P.H.F+150));
        HarPizzaBox(C,P.V(175,438,48),14);
    }
    else
    {
        const float X=(Free.Min.X+Free.Max.X)*.5f, Y=P.H.F+P.D*.25f;
        CrtTelevision(S,G,FVector(P.H.L+100,Y,P.Z),FVector(1,0,0),true);
        Sofa(S,K,FVector(X+50,Y,P.Z),180,Rect(X-15,Free.Max.X,Y-170,Y+170),I==2);
        Table(S,FVector(X-125,Y,P.Z),78,110,48);
        for(float Sign : {-1.f,1.f})
            Seat(S,K,FVector(X-120,Y+Sign*170,P.Z),Sign>0?-90:90,Rect(Free.Min.X,Free.Max.X,Y+Sign*170-55,Y+Sign*170+55));
        if(I==2) Fireplace(S,C,FVector(P.H.L+583,R.Max.Y,P.Z),FVector(0,-1,0),190);
        if(I!=0) FitKit(K,EKit::FloorLamp,FVector2D(Free.Min.X+50,R.Max.Y-150),P.Z,0,Rect(Free.Min.X,Free.Min.X+100,R.Max.Y-200,R.Max.Y-100));
    }
    // A wall-side bookcase occupies a fixed footprint away from stairs and the hall door.
    if(I!=4)
    {
        const FBox2D Case=I==3?Rect(R.Min.X+2,R.Min.X+50,P.H.F+330,P.H.F+470):Rect(R.Min.X+5,FMath::Min(R.Min.X+195,XR),P.Y1-60,P.Y1-15);
        const float Yaw=I==3?0.f:90.f;
        if(FitKit(K,EKit::Bookcase,Case.GetCenter(),P.Z,Yaw,Case))
            K.FillBookcase(Case.GetCenter(),P.Z,Yaw,I+4);
    }
    if(I==0) Framed(C,Key(I,TEXT("Notice")),FVector(P.H.L+350,R.Max.Y-3,P.Z+168),FVector(0,-1,0),92,68);
}

void Chapter(FHallMesh& S,FHallMesh& C,FHallMesh& G,const FKitter& K,const FPlan& P,int32 I)
{
    const FBox2D& R=P.Rooms[1];
    const float X=R.GetCenter().X+55, Y=R.Min.Y+(R.Max.Y-R.Min.Y)*(I<3?.69f:.62f);
    const float L=FMath::Min(440.f,R.Max.X-R.Min.X-(I<3?220.f:300.f));
    const FBox2D Middle=Rect(R.Min.X+170,R.Max.X-45,R.Min.Y+160,R.Max.Y-160);
    const bool Pool=I==2&&FitKit(K,EKit::BilliardTable,Middle.GetCenter(),P.Z,90,Middle);
    if(Pool)
    {
        // The first east casement previously sat behind this rack. The front-wall pier
        // is opaque, clear of its composite, and well south of the frozen room-entry route.
        const FBox2D Cue=Rect(R.Min.X+35,R.Min.X+180,P.H.F+33,P.H.F+78);
        FitKit(K,EKit::CueRack,Cue.GetCenter(),P.Z,90,Cue);
        Table(S,FVector(R.Min.X+240,R.Max.Y-65,P.Z),320,65,104);
        Boombox(S,FVector(R.Min.X+340,R.Max.Y-65,P.Z+104),FVector(0,-1,0));
        for(float Off : {-55.f,55.f})
        {
            const float CX=R.Min.X+240+Off,CY=R.Max.Y-65;
            FitKit(K,EKit::PaperCup,FVector2D(CX,CY),P.Z+104,0,Rect(CX-12,CX+12,CY-12,CY+12),true);
        }
    }
    else
    {
        Table(S,FVector(X,Y,P.Z),FMath::Max(180.f,L),80);
        const int32 Count=I<3?4:2;
        for(int32 N=0;N<Count;++N) for(float Side : {-1.f,1.f})
        {
            const float CX=X+(N-(Count-1)*.5f)*(L/FMath::Max(1,Count));
            Seat(S,K,FVector(CX,Y+Side*94,P.Z),Side>0?-90:90,Rect(CX-34,CX+34,Y+Side*94-36,Y+Side*94+36));
        }
        Typewriter(S,C,FVector(X+L*.31f,Y,P.Z+76),FVector(0,-1,0));
        DeskPhone(S,FVector(X-L*.31f,Y,P.Z+76),FVector(0,-1,0),true);
    }
    if(I!=2)
        Fireplace(S,C,FVector(X,R.Max.Y,P.Z),FVector(0,-1,0),170);
    // The case, awards, hand-made paddles and year composites establish a chapter rather than a hotel lounge.
    const FVector2D Pier=ChapterDisplayPier(P);
    const float Start=(Pier.X+Pier.Y-245.f)*.5f,Wall=P.H.R-31.5f;
    TrophyCase(S,G,FVector(Wall,Start+77.5f,P.Z),FVector(-1,0,0),155);
    for(int32 N=0;N<3;++N)
        // Top at 228cm stays below Tudor corbels starting at 230cm.
        Paddle(C,FVector(Wall-1.4f,Start+174+N*30,P.Z+185),FVector(-1,0,0),N);
    if(I>=3)
        // The compact front-wall window leaves no backing for the old 190cm composite.
        // Above the hearth: frame 144..278cm, between 140cm mantel and 284cm picture rail.
        Framed(C,Key(I,TEXT("Composite86")),FVector(X,R.Max.Y-3,P.Z+211),FVector(0,-1,0),170,128);
    else
        Framed(C,Key(I,TEXT("Composite86")),FVector(X,P.H.F+33,P.Z+195),FVector(0,1,0),190,142);
    if(I==4)
    {
        const FBox2D Case=Rect(R.Min.X,R.Min.X+48,P.H.F+420,P.H.F+560);
        if(FitKit(K,EKit::Bookcase,Case.GetCenter(),P.Z,0,Case))
            K.FillBookcase(Case.GetCenter(),P.Z,0,9);
    }
    Pendant(C,G,FVector(X,Y,P.Z+P.Story-18));
    if(I>=2)
    {
        // Suspended paper decoration has no collision and stays above head height.
        Streamer(C,FVector(R.Min.X+25,R.Min.Y+90,P.Z+270),FVector(R.Max.X-25,R.Max.Y-80,P.Z+278),true);
        if(I==2||I==4) HarPizzaBox(C,Pool?FVector(R.Min.X+240,R.Max.Y-65,P.Z+104):FVector(X,Y,P.Z+76),I*17);
    }
}

void Dining(FHallMesh& S,FHallMesh& C,const FKitter& K,const FPlan& P,int32 I)
{
    const FBox2D& R=P.Rooms[2];
    const float L=FMath::Min(650.f,R.Max.X-R.Min.X-(I>=3?340.f:300.f)), X=R.Min.X+80+L*.5f, Y=R.GetCenter().Y;
    const int32 Count=I<3?4:2;
    Table(S,FVector(X,Y,P.Z),L,90);
    for(int32 N=0;N<Count;++N) for(float Side : {-1.f,1.f})
    {
        const float CX=X+(N-(Count-1)*.5f)*(L/Count), CY=Y+Side*(I>=3?85.f:100.f), HalfY=I>=3?32.f:38.f;
        Seat(S,K,FVector(CX,CY,P.Z),Side>0?-90:90,Rect(CX-35,CX+35,CY-HalfY,CY+HalfY));
    }
    for(int32 N=0;N<(I==0?1:I==1?2:4);++N)
    {
        const FVector At(X-L*.35f+N*L*.21f,Y+(N%2?22.f:-22.f),P.Z+76);
        FitKit(K,EKit::PaperCup,FVector2D(At.X,At.Y),At.Z,0,Rect(At.X-12,At.X+12,At.Y-12,At.Y+12),true);
        if(I>=2) HarCan(C,At+FVector(18,0,0),N%2==0,I==2&&N==3,N*37);
    }
    if(I>=2) HarPizzaBox(C,FVector(X,Y,P.Z+76),I*11);
    Framed(C,Key(I,TEXT("Notice")),FVector(R.Min.X,Y,P.Z+168),FVector(1,0,0),90,67);
    if(I==4)
    {
        // Food is left where people ate; the parlor's 200 cm connecting opening stays clear.
        C.Cylinder(TEXT("CI_RowPorcelain"),FVector(X+30,Y+15,P.Z+76),FVector(X+30,Y+15,P.Z+77.5f),13,14,UVPlain());
        C.Dome(TEXT("CI_Bun"),FVector(X+30,Y+15,P.Z+77.5f),FVector::UpVector,8,4,10);
    }
}

void Counter(FHallMesh& S,FHallMesh& C,float X0,float X1,float Y0,float Y1,float Z)
{
    Box(S,TEXT("CI_RowEnamel"),FVector(X0,Y0,Z+8),FVector(X1,Y1,Z+86));
    Box(S,TEXT("CI_RowPorcelain"),FVector(X0-1,Y0-1,Z+86),FVector(X1+1,Y1+1,Z+91));
    for(float X=X0+20;X<X1-10;X+=50)
        Box(C,TEXT("CI_OakDark"),FVector(X,Y0-.2f,Z+15),FVector(FMath::Min(X+1,X1),Y0-.1f,Z+80));
}

void Kitchen(FHallMesh& S,FHallMesh& C,FHallMesh& G,const FKitter& K,const FPlan& P,int32 I)
{
    const FBox2D& R=P.Rooms[3];
    const float Back=R.Max.Y, East=R.Max.X;
    Counter(S,C,R.Min.X+35,East-145,Back-65,Back,P.Z);
    const float StoveY=R.Min.Y+140;
    Counter(S,C,East-64,East,R.Min.Y+25,StoveY-50,P.Z);
    Counter(S,C,East-64,East,StoveY+50,Back-215,P.Z);
    // Tall rounded-era enamel refrigerator, a stove/oven, a sink and open supplies.
    Box(S,TEXT("CI_RowEnamel"),FVector(East-130,Back-122,P.Z),FVector(East-43,Back-35,P.Z+183));
    Box(C,TEXT("CI_RowJoint"),FVector(East-130.4f,Back-122.4f,P.Z+130),FVector(East-42.6f,Back-122,P.Z+131));
    for(float Z : {83.f,153.f})
        C.Cylinder(TEXT("CI_Steel"),FVector(East-118,Back-125,P.Z+Z-15),FVector(East-118,Back-125,P.Z+Z+15),1.5f,6,UVPlain());
    Box(S,TEXT("CI_RowPorcelain"),FVector(East-72,StoveY-45,P.Z),FVector(East,StoveY+45,P.Z+92));
    for(float X : {East-53,East-22}) for(float Y : {StoveY-23,StoveY+23})
        C.Cylinder(TEXT("CI_RowJoint"),FVector(X,Y,P.Z+92),FVector(X,Y,P.Z+93),10,12,UVPlain());
    Box(C,TEXT("CI_RowJoint"),FVector(East-72.5f,StoveY-31,P.Z+22),FVector(East-72,StoveY+31,P.Z+67));
    C.Cylinder(TEXT("CI_Steel"),FVector(East-76,StoveY-31,P.Z+76),FVector(East-76,StoveY+31,P.Z+76),1.6f,6,UVPlain());
    const float SinkX=R.Min.X+140;
    Box(C,TEXT("CI_Steel"),FVector(SinkX-33,Back-56,P.Z+91.1f),FVector(SinkX+33,Back-10,P.Z+92));
    Box(C,TEXT("CI_RowJoint"),FVector(SinkX-26,Back-50,P.Z+92.1f),FVector(SinkX+26,Back-16,P.Z+92.3f));
    C.Cylinder(TEXT("CI_Steel"),FVector(SinkX,Back-7,P.Z+91),FVector(SinkX,Back-7,P.Z+122),1.7f,8,UVPlain());
    C.Cylinder(TEXT("CI_Steel"),FVector(SinkX,Back-7,P.Z+122),FVector(SinkX,Back-30,P.Z+122),1.7f,8,UVPlain());
    for(int32 N=0;N<5;++N)
        C.Cylinder(TEXT("CI_RowPorcelain"),FVector(SinkX+60,Back-30,P.Z+92+N*1.6f),FVector(SinkX+60,Back-30,P.Z+93+N*1.6f),14,12,UVPlain());
    // Pipes, a kettle and wall-side shelves; no modern appliances or visible LED equipment.
    C.Cylinder(TEXT("CI_Steel"),FVector(East-22,StoveY+23,P.Z+94),FVector(East-22,StoveY+23,P.Z+112),9,12,UVPlain());
    WindowAwareShelves(S,C,P,R);
    if(R.Max.X-R.Min.X>900)
    {
        const FVector At(R.GetCenter().X,R.GetCenter().Y,P.Z);
        Table(S,At,200,80,91);
        FitKit(K,EKit::PaperStack,FVector2D(At.X+55,At.Y),P.Z+91,0,Rect(At.X+30,At.X+80,At.Y-25,At.Y+25),true);
    }
    const FBox2D Rack=Rect(R.Min.X+20,R.Min.X+155,R.Min.Y+20,R.Min.Y+180);
    FitKit(K,EKit::WireRack,Rack.GetCenter(),P.Z,0,Rack);
    Troffer(G,C,FVector(R.GetCenter(),P.Z+P.Story-18),true);
    FitKit(K,EKit::TrashCan,FVector2D(East-45,R.Min.Y+260),P.Z,0,Rect(East-85,East-5,R.Min.Y+220,R.Min.Y+300));
    if(I>=2)
    {
        HarPizzaBox(C,FVector(East-35,R.Min.Y+340,P.Z+91),7*I);
        HarCan(C,FVector(East-40,R.Min.Y+400,P.Z+91),true,false,0);
    }
}

float StallFront(const FPlan& P) { return P.Rooms[4].Max.Y-190.f; }
float StallWidth(const FPlan& P) { return P.H.Index==4?125.f:155.f; }
float StallLeft(const FPlan& P) { return P.Rooms[4].Min.X+(P.H.Index==4?40.f:60.f); }
void WC(FHallMesh& S,FHallMesh& C,FHallMesh& G,const FPlan& P)
{
    const FBox2D& R=P.Rooms[4]; const float Front=StallFront(P), XL=StallLeft(P), Cell=StallWidth(P);
    for(int32 N=0;N<2;++N)
    {
        const float X=XL+N*Cell, Center=X+Cell*.5f, Back=R.Max.Y;
        // Two ordinary cubicles and a separate clear approach from the hall.
        BlockWall(S,TEXT("CI_RowEnamel"),false,Front,X,X+Cell,P.Z+12,P.Z+205,{{Center-42,Center+42}},193);
        if(N==0) BlockWall(S,TEXT("CI_RowEnamel"),true,XL,Front,Back,P.Z+12,P.Z+205,{});
        BlockWall(S,TEXT("CI_RowEnamel"),true,X+Cell,Front,Back,P.Z+12,P.Z+205,{});
        Box(S,TEXT("CI_RowPorcelain"),FVector(Center-21,Back-29,P.Z+30),FVector(Center+21,Back-4,P.Z+87));
        S.Cylinder(TEXT("CI_RowPorcelain"),FVector(Center,Back-66,P.Z+5),FVector(Center,Back-66,P.Z+36),17,14,UVPlain());
        S.Dome(TEXT("CI_RowPorcelain"),FVector(Center,Back-66,P.Z+38),FVector::UpVector,23,6,14);
        C.Cylinder(TEXT("CI_RowJoint"),FVector(Center,Back-66,P.Z+44),FVector(Center,Back-66,P.Z+44.2f),15,14,UVPlain());
        Box(C,TEXT("CI_RowPorcelain"),FVector(X+9,Back-92,P.Z+60),FVector(X+20,Back-77,P.Z+73));
    }
    const float SinkY=R.Min.Y+65;
    Box(S,TEXT("CI_RowPorcelain"),FVector(R.Min.X,SinkY-28,P.Z+70),FVector(R.Min.X+53,SinkY+28,P.Z+84));
    C.Dome(TEXT("CI_Steel"),FVector(R.Min.X+28,SinkY,P.Z+84.2f),FVector::UpVector,18,1,12);
    C.Cylinder(TEXT("CI_Steel"),FVector(R.Min.X+6,SinkY,P.Z+84),FVector(R.Min.X+6,SinkY,P.Z+110),1.5f,7,UVPlain());
    C.Cylinder(TEXT("CI_Steel"),FVector(R.Min.X+6,SinkY,P.Z+110),FVector(R.Min.X+25,SinkY,P.Z+110),1.5f,7,UVPlain());
    if(P.H.Index>=3)
        // A corner wash area: turn the mirror onto the opaque south return instead of
        // mounting it across the compact houses' west casement. Sink/route stay fixed.
        Box(C,TEXT("CI_RowMirror"),FVector(R.Min.X+10,R.Min.Y+.5f,P.Z+112),FVector(R.Min.X+74,R.Min.Y+1,P.Z+190));
    else
        Box(C,TEXT("CI_RowMirror"),FVector(R.Min.X+.5f,SinkY-32,P.Z+112),FVector(R.Min.X+1,SinkY+32,P.Z+190));
    Troffer(G,C,FVector(R.Max.X-80,R.GetCenter().Y,P.Z+P.Story-18),false);
}

void LivedIn(FHallMesh& S,FHallMesh& C,const FKitter& K,const FPlan& P,int32 I)
{
    const FBox2D& Chapter=P.Rooms[1];
    const float X=Chapter.GetCenter().X+55, Y=Chapter.Min.Y+(Chapter.Max.Y-Chapter.Min.Y)*(I<3?.69f:.62f);
    // These are useful loose belongings on actual support surfaces, not population-test filler.
    if(I!=2)
    {
        const FBox2D Papers=Rect(X-25,X+25,Y-20,Y+20);
        const bool Stack=FitKit(K,EKit::PaperStack,FVector2D(X,Y),P.Z+76,0,Papers,true);
        FitKit(K,EKit::RedBook,FVector2D(X,Y),P.Z+76+(Stack?K.Height(EKit::PaperStack):0),0,Papers,true);
        const float CupX=X-48;
        FitKit(K,EKit::PaperCup,FVector2D(CupX,Y-20),P.Z+76,0,Rect(CupX-11,CupX+11,Y-31,Y-9),true);
    }
    const FBox2D& Kitchen=P.Rooms[3];
    for(int32 N=0;N<2;++N)
    {
        const float CX=Kitchen.Min.X+70+N*110, CY=Kitchen.Max.Y-165;
        FitKit(K,N==0?EKit::Cardboard:EKit::StorageBox,FVector2D(CX,CY),P.Z,0,Rect(CX-43,CX+43,CY-43,CY+43),true);
    }
    const FVector2D Bag=I==4?FVector2D(P.H.L+350,P.H.F+650):I==3?FVector2D(P.H.L+350,P.H.F+520):FVector2D(P.H.L+105,P.H.F+110);
    FitKit(K,EKit::Backpack,Bag,P.Z,0,Rect(Bag.X-38,Bag.X+38,Bag.Y-38,Bag.Y+38),true);
    if(I>=3)
    {
        const FVector2D Book=I==4?FVector2D(P.H.L+105,P.H.F+318):FVector2D(P.H.L+175,P.H.F+440);
        const float Top=I==4?61.f:48.f;
        const bool OnTable=FitKit(K,EKit::RedBook,Book,P.Z+Top,0,Rect(Book.X-28,Book.X+28,Book.Y-22,Book.Y+22),true);
        if(I==4&&OnTable)
            FitKit(K,EKit::PaperCup,Book,P.Z+Top+K.Height(EKit::RedBook),0,Rect(Book.X-10,Book.X+10,Book.Y-10,Book.Y+10),true);
    }
    // The real 180x245cm rug runs along the hall, 20cm from its finished sides. Its south
    // edge at 152.5cm stays behind the main leaves' complete 100cm swing ending near 130cm.
    FitKit(K,EKit::EntryRug,FVector2D(P.C,P.H.F+275),P.Z+.08f,90,Rect(P.LHall+4,P.RHall-4,P.H.F+145,P.H.F+405));
    const FBox2D& Bath=P.Rooms[4];
    const FVector2D Basket(Bath.Max.X-80,Bath.Max.Y-33);
    FitKit(K,EKit::LaundryBasket,Basket,P.Z,0,Rect(Basket.X-38,Basket.X+38,Basket.Y-28,Basket.Y+28),true);
    // Ash urns and folded supply linens stay at the wall, clear of doors and cubicles.
    if(I>=2) AshUrn(S,FVector(Chapter.Max.X-35,Chapter.Min.Y+75,P.Z));
}

void QuietHouse(FHallMesh& C,FHallMesh& G,const FPlan& P)
{
    // Match the permanent shell's current ground-window schedule, excluding the door bay.
    const float Bottom=P.H.Floor+40,Top=P.H.Floor+235;
    for(int32 N=0;N<5;++N) if(N!=2)
    {
        const float X=FMath::Lerp(P.H.L+230,P.H.R-230,N*.25f);
        for(float Y : {P.H.F+33.f,P.H.B-35.f})
            Box(C,TEXT("CI_RowQuietCurtain"),FVector(X-84,Y,Bottom),FVector(X+84,Y+2,Top));
    }
    for(int32 N=0;N<4;++N)
    {
        const float Y=FMath::Lerp(P.H.F+280,P.H.B-280,N/3.f);
        for(float X : {P.H.L+33.f,P.H.R-35.f})
            Box(C,TEXT("CI_RowQuietCurtain"),FVector(X,Y-84,Bottom),FVector(X+2,Y+84,Top));
    }
    // Keep the actual fixture geometry while replacing every ground lens/screen's emitting material.
    for(FName& Mat : G.Order)
    {
        const FName Dead(*FString::Printf(TEXT("CI_RowDead_%s"),*Mat.ToString()));
        FHallSection Section=MoveTemp(G.Sections.FindChecked(Mat));
        G.Sections.Remove(Mat); G.Sections.Add(Dead,MoveTemp(Section)); Mat=Dead;
    }
}

template<int32 I> void Build(int32 Seed,float Height,FHallMesh& S,FHallMesh& C,FHallMesh& G,const FKitter& K)
{
    const FPlan P(I);
    Shell(S,C,G,P,I); Lounge(S,C,G,K,P,I); Chapter(S,C,G,K,P,I);
    Dining(S,C,K,P,I); Kitchen(S,C,G,K,P,I); WC(S,C,G,P); LivedIn(S,C,K,P,I);
    if(I==0)
    {
        QuietHouse(C,G,P);
        Pendant(C,G,FVector(P.UpperLanding.GetCenter(),P.Z+P.Story+230));
    }
    if(I==2)
    {
        // Isolated marks sit under party activity, never across a doorway or escape route.
        for(int32 N=0;N<4;++N)
            C.Dome(TEXT("CI_RowSpill"),P.V(P.W-230-N*17,350+N*82,.12f),FVector::UpVector,24+N*3,.04f,12);
    }
}

template<int32 I> void Openings(int32 Seed,float Height,TArray<HouseCampusInteriors::FOpening>& Out)
{
    const FPlan P(I);
    Out.Add({1,P.C-100,P.C+100,P.H.Floor,P.H.Floor+OuterDoorH});
    Out.Add({0,P.C-70,P.C+70,P.H.Floor,P.H.Floor+OuterDoorH});
}

template<int32 I> void Doors(int32 Seed,float Height,TArray<FDoorSpec>& Out)
{
    const FPlan P(I); const FBox2D R=Rect(P.H.L,P.H.R,P.H.F,P.H.B);
    TArray<HouseCampusInteriors::FOpening> Holes; Openings<I>(Seed,Height,Holes);
    const int32 Start=Out.Num();
    SidePair(P.H.Id,TEXT("Main"),R,Holes[0],FString(),Out);
    SidePair(P.H.Id,TEXT("Rear"),R,Holes[1],FString(),Out);
    for(int32 N=Start;N<Out.Num();++N)
    {
        Out[N].Height=OuterDoorH;
        Out[N].bOpen=I==4&&N<Start+2;
        if(N>=Start+2) Out[N].Style=EHallDoorStyle::Equipment;
    }
    const TCHAR* Names[]={TEXT("Lounge"),TEXT("Chapter"),TEXT("Dining"),TEXT("Kitchen"),TEXT("SharedWC")};
    for(int32 N=0;N<5;++N)
    {
        const bool Left=N==0||N==2||N==4;
        InnerDoor(Out,FName(Names[N]),true,Left?P.LW:P.RW,P.DoorY[N]-70,P.DoorY[N]+70,P.Z,
            Left?-1.f:1.f,N>=3?EHallDoorStyle::Equipment:EHallDoorStyle::French,
            uint8(AHouseBlackHallDoor::Lever),FString(),I==2&&N!=4);
    }
    if(I==4)
    {
        const float Y=P.Y1-PartT*.5f-1;
        const FName Group(TEXT("Campus_R5_Parlor"));
        const FVector Center(P.H.L+200,P.Y1,P.Z+105);
        Out.Add({TEXT("ParlorA"),FVector(P.H.L+100.4f,Y,P.Z),0,EHallDoorStyle::French,99.4f,-1,uint8(AHouseBlackHallDoor::Lever),Center,Group,FString()});
        Out.Add({TEXT("ParlorB"),FVector(P.H.L+299.6f,Y,P.Z),180,EHallDoorStyle::French,99.4f,1,uint8(AHouseBlackHallDoor::Lever),Center,Group,FString()});
    }
    for(int32 N=0;N<2;++N)
    {
        const float Center=StallLeft(P)+(N+.5f)*StallWidth(P);
        InnerDoor(Out,FName(*FString::Printf(TEXT("WCStall%d"),N+1)),false,StallFront(P),Center-42,Center+42,P.Z+12,1,
            EHallDoorStyle::Equipment,uint8(AHouseBlackHallDoor::Lever),FString());
        Out.Last().Height=193;
    }
    InnerDoor(Out,TEXT("UpperPrivate"),false,P.UpperLanding.Max.Y+PartT*.5f,P.UpperLanding.Min.X+7,P.UpperLanding.Max.X-7,
        P.Z+P.Story,1,EHallDoorStyle::Plank,uint8(AHouseBlackHallDoor::Lever|AHouseBlackHallDoor::Deadbolt),TEXT("RESIDENT ROOMS - PRIVATE AFTER HOMECOMING"));
}

template<int32 I> void Anchors(int32 Seed,float Height,TArray<FVector>& Out)
{
    const FPlan P(I); const float Z=P.Z+245;
    if(I==0)
    {
        // Deliberately dark house: no ground anchors; one upper landing lamp only.
        Out.Add(FVector(P.UpperLanding.GetCenter(),P.Z+P.Story+200));
        return;
    }
    // Candidate positions for the shared pool of six; this function creates no light components.
    Out.Add(FVector(P.C,P.H.F+P.D*.28f,Z));
    Out.Add(FVector(P.C,P.H.F+P.D*.69f,Z));
    for(int32 N=0;N<5;++N)
    {
        FVector2D At=P.Rooms[N].GetCenter();
        if(N==0) At.X=FMath::Min(At.X,P.Stairs.Min.X-95);
        if(N==4) At=FVector2D(P.Rooms[4].Max.X-90,P.Rooms[4].Min.Y+70);
        Out.Add(FVector(At,Z));
    }
    Out.Add(FVector(P.UpperLanding.GetCenter(),P.Z+P.Story+200));
}

template<int32 I> FBox Bounds()
{
    const FPlan P(I);
    return FBox(FVector(P.H.L,P.H.F,P.H.Floor-20),FVector(P.H.R,P.H.B,P.Z+P.Story+252));
}
template<int32 I> bool Inside(const FVector& W)
{
    const FPlan P(I);
    const bool CornerInside=I!=4||W.X>=P.H.L+150||W.Y>=P.H.F+150||FVector2D::DistSquared(FVector2D(W.X,W.Y),FVector2D(P.H.L+150,P.H.F+150))<FMath::Square(118.5f);
    const bool Ground=CornerInside&&W.X>P.H.L+30&&W.X<P.H.R-30&&W.Y>P.H.F+30&&W.Y<P.H.B-30&&W.Z>=P.H.Floor-3&&W.Z<P.Z+P.Story-18;
    const bool Stair=W.X>P.Stairs.Min.X-8&&W.X<P.Stairs.Max.X+8&&W.Y>P.Stairs.Min.Y-8&&W.Y<P.RunEnd+1&&W.Z>=P.H.Floor&&W.Z<P.Z+P.Story+225;
    const bool Landing=W.X>P.UpperLanding.Min.X&&W.X<P.UpperLanding.Max.X&&W.Y>=P.UpperLanding.Min.Y&&W.Y<P.UpperLanding.Max.Y&&W.Z>=P.Z+P.Story-3&&W.Z<P.Z+P.Story+240;
    return Ground||Stair||Landing;
}
template<int32 I> float FloorZ(int32 Floor)
{ const FPlan P(I); return P.Z+Floor*P.Story; }

template<int32 I> FInteriorDef Def()
{
    FInteriorDef D{HouseFraternityRow::Houses()[I].Id,2,&Build<I>,&Anchors<I>,&Openings<I>,&Doors<I>,&Look,&Bounds<I>,&Inside<I>,&FloorZ<I>};
    D.bLightsInsideOnly=true;
    D.bPlayerProximityOnly=true;
    // Integration adds this defaulted field to FInteriorDef; existing definitions retain300cm.
    D.LightAnchorFloorOffset=200.f;
    return D;
}
} // namespace Row

void RowDefs(TArray<FInteriorDef>& Out)
{
    if(!HouseFraternityRow::Enabled()||HouseFraternityRow::Houses().Num()!=5) return;
    Out.Add(Row::Def<0>()); Out.Add(Row::Def<1>()); Out.Add(Row::Def<2>());
    Out.Add(Row::Def<3>()); Out.Add(Row::Def<4>());
}
} // namespace HouseCampusInteriorsImpl
