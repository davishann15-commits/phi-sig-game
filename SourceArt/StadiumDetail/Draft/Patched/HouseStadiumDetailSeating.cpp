#include "HouseStadiumDetail.h"
#include "HouseCampusInteriors.h"
#include "HouseBlackHallPaint.h"
#include "HouseBlackHall.h"
#include "HouseCampusFallKit.h"
namespace HouseStadiumDetailSeatingImpl
{
constexpr float CX=2400.f,CY=-13300.f,RX=5600.f,RY=3800.f,G=-60.f,HalfAisle=130.f;
FVector At(float A,float R,float Z){return {CX+FMath::Cos(A)*RX*R,CY+FMath::Sin(A)*RY*R,Z};}
float Angle(int32 K){return K*2.f*PI/40.f;}
void Panel(FHallMesh& M,FName Key,FVector A,FVector B,float Lo,float Hi,FVector N)
{A.Z=Lo;B.Z=Lo;M.Quad(Key,A,B,B+FVector(0,0,Hi-Lo),A+FVector(0,0,Hi-Lo),N,FHallUV::World(180));}
float Gap(int32 K,float Radius){return (HalfAisle+5.f)/FVector::Dist(At(Angle(K),Radius,0),At(Angle(K+1),Radius,0));}
FVector Mid(int32 K,float R,float Z){return (At(Angle(K),R,Z)+At(Angle(K+1),R,Z))*.5f;}
void Step(FHallMesh& M,FName Key,FVector Start,FVector End,FVector Cross,float Half,float Bottom,float Top)
{Start.Z=End.Z=Top;const FVector A=Start-Cross*Half,B=Start+Cross*Half,C=End+Cross*Half,D=End-Cross*Half,Down(0,0,Bottom-Top);FVector Front=FVector::CrossProduct(Cross,FVector::UpVector).GetSafeNormal(),Side=FVector::CrossProduct(End-Start,FVector::UpVector).GetSafeNormal();if(FVector::DotProduct(Front,End-Start)<0)Front=-Front;if(FVector::DotProduct(Side,Cross)>0)Side=-Side;const auto UV=FHallUV::World(300);M.Quad(Key,A,B,C,D,FVector::UpVector,UV);M.Quad(Key,A+Down,D+Down,C+Down,B+Down,-FVector::UpVector,UV);M.Quad(Key,A+Down,B+Down,B,A,-Front,UV);M.Quad(Key,D+Down,C+Down,C,D,Front,UV);M.Quad(Key,A+Down,D+Down,D,A,Side,UV);M.Quad(Key,B+Down,C+Down,C,B,-Side,UV);}
void Number(FHallMesh& M,int32 Index,FVector C,FVector Right,FVector Out)
{const FString Text=FString::Printf(TEXT("%02d"),Index+1);for(int32 I=0;I<2;++I)for(const auto& Stroke:HallGlyph(Text[I]))for(int32 J=1;J<Stroke.Num();++J){auto P=[&](FVector2D Q){return C+Right*((I-.5f)*5.f+Q.X*.7f-1.4f)+Out*(Q.Y*.7f-2.1f);};const FVector A=P(Stroke[J-1]),B=P(Stroke[J]),D=(B-A).GetSafeNormal(),Side(-D.Y,D.X,0);M.Quad(TEXT("SD_Dark"),A-Side*.24f,B-Side*.24f,B+Side*.24f,A+Side*.24f,FVector::UpVector,FHallUV::World(100));}}
}
void HouseStadiumDetail::Tier(FHallMesh& M,int32 K,int32 Row,FName)
{
 namespace S=HouseStadiumDetailSeatingImpl;const float A=S::Angle(K),B=S::Angle(K+1),R0=.69f+Row*.024f,R1=R0+.024f,Z=180.f+Row*63.f;
 const FVector Out=FVector(FMath::Cos((A+B)*.5f),FMath::Sin((A+B)*.5f),0);
 auto Piece=[&](float T0,float T1){auto P=[&](float R,float T,float H){return FMath::Lerp(S::At(A,R,H),S::At(B,R,H),T);};M.Quad(TEXT("EX_CampusSeats"),P(R0,T0,Z),P(R0,T1,Z),P(R1,T1,Z),P(R1,T0,Z),FVector::UpVector,FHallUV::World(300));S::Panel(M,TEXT("SD_Concrete"),P(R1,T0,0),P(R1,T1,0),Z-63,Z,Out);if(Row==0&&HouseCampusInteriors::StadiumTunnel(K))HouseCampusInteriors::StadiumTunnelWall(M,K,P(R0,T0,-60.f),P(R0,T1,-60.f),-Out,Z+60.f);else S::Panel(M,TEXT("SD_Concrete"),P(R0,T0,0),P(R0,T1,0),Row==0?-60.f:Z-63.f,Z,-Out);if(Row==11)S::Panel(M,TEXT("SD_Concrete"),P(R1,T0,0),P(R1,T1,0),Z,990,-Out);};
 if(!Aisle(K)&&!PortalBand(K,Row)){Piece(0,1);return;}
 const float Gap=(PortalBand(K,Row)?175.f:S::HalfAisle+5.f)/FVector::Dist(S::At(A,R0,0),S::At(B,R0,0));Piece(0,.5f-Gap);Piece(.5f+Gap,1);
 // Four intermediate treads per unchanged63cm tier. Chosen end-bowl aisles have>=120cm radial run.
 if(Aisle(K)&&Row<11){const FVector Start=S::Mid(K,R0,Z),End=S::Mid(K,R1,Z+63.f),D=(End-Start).GetSafeNormal2D(),Cross=(S::At(B,R0,0)-S::At(A,R0,0)).GetSafeNormal();const float Run=FVector::Dist2D(Start,End)/4.f;
  for(int32 J=0;J<4;++J){const FVector P0=Start+D*(J*Run),P1=Start+D*((J+1)*Run);const float Top=Z+(J+1)*15.75f;S::Step(M,TEXT("EX_CampusSeats"),P0,P1,Cross,S::HalfAisle,Z-8,Top);S::Step(M,TEXT("SD_Gold"),P0,P0+D*4.f,Cross,S::HalfAisle-2,Top,Top+.2f);}
 }
}
void HouseStadiumDetail::Rim(FHallMesh& M,int32 K,FName Stone)
{
 namespace S=HouseStadiumDetailSeatingImpl;const float A=S::Angle(K),B=S::Angle(K+1),Gap=S::Gap(K,.98f);auto P=[&](float R,float T){return FMath::Lerp(S::At(A,R,990),S::At(B,R,990),T);};for(auto T: {FVector2D(0,.5f-Gap),FVector2D(.5f+Gap,1)})M.Quad(Stone,P(.98f,T.X),P(.98f,T.Y),P(1.045f,T.Y),P(1.045f,T.X),FVector::UpVector,FHallUV::World(300));
 // End the ascent before the fixed outer guard, with a supported top landing inside the original rim footprint.
 M.Quad(Stone,P(1.025f,.5f-Gap),P(1.025f,.5f+Gap),P(1.045f,.5f+Gap),P(1.045f,.5f-Gap),FVector::UpVector,FHallUV::World(300));
}
void AHouseStadiumDetail::Seating()
{
 namespace S=HouseStadiumDetailSeatingImpl;FRandomStream Rand(861018);int32 Planks=0;
 // Collision follows the existing visible turf exactly. The legacy terrain is4cm below it; do not move either surface.
 {FHallMesh Support;TArray<FVector> Rim;for(int32 I=0;I<96;++I)Rim.Add(S::At(I*2*PI/96,.668f,-56));HouseCampusFallKit::Fan(Support,TEXT("SD_Concrete"),{2400,-13300,-56},Rim,FVector::UpVector);auto* P=NewObject<UProceduralMeshComponent>(this,TEXT("CalderStadiumTurfSupport"));AddInstanceComponent(P);P->SetupAttachment(RootComponent);P->SetMobility(EComponentMobility::Static);P->SetCastShadow(false);P->bAffectDynamicIndirectLighting=false;P->bAffectDistanceFieldLighting=false;P->bUseComplexAsSimpleCollision=true;P->bUseAsyncCooking=false;P->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);P->SetCollisionObjectType(ECC_WorldStatic);P->SetCollisionResponseToAllChannels(ECR_Block);P->SetCanEverAffectNavigation(false);P->RegisterComponent();Support.Build(P,[this](FName K){return Look->Mat(K);},true);P->SetVisibility(false);}
 for(int32 K=0;K<40;++K)
 {
  const float A=S::Angle(K),B=S::Angle(K+1);const FVector Along=(S::At(B,1,0)-S::At(A,1,0)).GetSafeNormal(),Out(Along.Y,-Along.X,0);const FRotator Rot=Along.Rotation();
  for(int32 Row=0;Row<12;++Row)
  {
   if(HouseCampusInteriors::StadiumVomitory(K,Row))continue;const float Rad=.70f+Row*.024f,Z=180.f+Row*63.f;const FVector Left=S::At(A,Rad,Z),Right=S::At(B,Rad,Z);const float Length=FVector::Dist2D(Left,Right);const float Gap=HouseStadiumDetail::Aisle(K)?S::HalfAisle+10.f:0.f;
   // Short sections are joined at6mm saw gaps; the open under-seat space has real stanchions and bolted feet.
   for(float U=6.f;!HouseStadiumDetail::CrossAisle(K,Row)&&U<Length-35.f;)
   {
    const float Clear=HouseStadiumDetail::PortalBand(K,Row)?192.f:Gap;
    float End=FMath::Min(U+175.f,Length-6.f);if(Clear>0.f){if(U>=Length*.5f-Clear&&U<Length*.5f+Clear){U=Length*.5f+Clear+.6f;continue;}if(U<Length*.5f-Clear&&End>Length*.5f-Clear)End=Length*.5f-Clear-.6f;}
    const float Span=End-U;if(Span<40.f){if(Clear>0.f&&U<Length*.5f){U=Length*.5f+Clear+.6f;continue;}break;}const FVector C=Left+Along*((U+End)*.5f);const bool Student=K>=22&&K<=26;const bool Wood=(K>=16&&K<=21)||Student;const FName Seat=Wood?FName(TEXT("SD_SeatWood")):FName(TEXT("SD_Aluminium"));
    for(const float D:{-8.2f,8.2f})Plank(Seat,C+Out*D+FVector(0,0,43),Span,Rot);
    // Rolled edges and longitudinal extrusion ribs distinguish aluminium from a painted rectangular strip.
    if(!Wood)for(float D:{-16.4f,16.4f})Box(TEXT("SD_Aluminium"),C+Out*D+FVector(0,0,42),{Span,1.1f,3.8f},Rot,false,16000);
    for(float T:{U+15.f,End-15.f}){const FVector P=Left+Along*T;Box(TEXT("SD_Steel"),P+FVector(0,0,20),{4,8,40},Rot);Box(TEXT("SD_Steel"),P+FVector(0,0,39.5f),{5,38,3},Rot);Box(TEXT("SD_Steel"),P+FVector(0,0,1.3f),{15,22,2.6f},Rot);for(float DU:{-5.f,5.f})for(float DV:{-8.f,8.f})Box(TEXT("SD_Rust"),P+Along*DU+Out*DV+FVector(0,0,3.1f),{2.5f,2.5f,1.1f},Rot,false,4500);}
    for(float T=U+22.f;T<End-12.f;T+=45.f){const int32 Number=FMath::Clamp(int(T/45.f)%32,0,31);S::Number(Unique,Number,Left+Along*T+FVector(0,0,45.08f),Along,Out);}
    ++Planks;U=End+.6f;
   }
   // Expansion joints lie flush on the tread. No repeated component per detail.
   const FVector Joint=S::At(A+.002f,.69f+Row*.024f,Z+.07f),JointEnd=S::At(A+.002f,.7135f+Row*.024f,Z+.07f);Pipe(TEXT("SD_Dark"),Joint,JointEnd,.35f,false,8500);
   if(K%4==1){const FVector Mark=S::Mid(K,.69f+Row*.024f,Z-25.f)-Out*.1f;Letter(FString::Chr(TCHAR('A'+Row)),Mark,Along,-Out,14.f);}
   if(Row>0&&K%3==1&&!HouseStadiumDetail::Aisle(K)&&!HouseStadiumDetail::PortalBand(K,Row)){const FVector P=S::Mid(K,.69f+Row*.024f,Z)-Out*.13f+Along*Rand.FRandRange(-80,80);const float Width=Rand.FRandRange(1.5f,4.f),Drop=Rand.FRandRange(17,42);Unique.Quad(TEXT("SD_Rust"),P-Along*Width,P+Along*Width,P+Along*.8f-FVector(0,0,Drop),P-Along*.6f-FVector(0,0,Drop*.92f),-Out,FHallUV::World(100));}
   if(Row>0&&K%7==3&&!HouseStadiumDetail::Aisle(K)&&!HouseStadiumDetail::PortalBand(K,Row)){const FVector P=S::Mid(K,.69f+Row*.024f,Z)-Out*.16f+Along*Rand.FRandRange(-100,100);const float Width=Rand.FRandRange(15,35),Drop=Rand.FRandRange(25,60);Unique.Quad(TEXT("SD_WaterStain"),P-Along*Width,P+Along*Width,P+Along*(Width*.30f)-FVector(0,0,Drop),P-Along*(Width*.6f)-FVector(0,0,Drop*.83f),-Out,FHallUV::World(100));}
   // The tunnel opening ends at worldZ160; this front-row guard starts at180, above its lintel.
   if(Row==0)
   {
    const FVector P0=S::At(A,.690f,Z),P1=S::At(B,.690f,Z);if(!HouseStadiumDetail::Aisle(K))Rail(P0,P1);else{const float G=S::Gap(K,.69f);Rail(P0,FMath::Lerp(P0,P1,.5f-G));Rail(FMath::Lerp(P0,P1,.5f+G),P1);}
   }
   // Gum and bolt runoff: a restrained handful per row, instanced and distance culled.
   for(int32 J=0;!HouseStadiumDetail::PortalBand(K,Row)&&J<(K>=22&&K<=26?5:2);++J){const float T=Rand.FRandRange(.05f,.95f);if(Gap>0.f&&FMath::Abs(T-.5f)*Length<Gap+8)continue;const FVector P=FMath::Lerp(Left,Right,T)+Out*Rand.FRandRange(25.f,60.f);Box(TEXT("SD_Gum"),P+FVector(0,0,.12f),{Rand.FRandRange(1,4),Rand.FRandRange(1,3),.2f},FRotator(0,Rand.FRandRange(0,180),0),false,6500);}
  }
  if(K==2||K==17)
  {
   // Three legal side steps join the row-D cross aisle to the middle of the existing ascending aisle.
   // A flat row at369 would otherwise meet a47.25cm side rise here.
   const float Side=K==2?1.f:-1.f;const FVector Base=S::Mid(K,.774f,369),P0=Base+Along*(Side*235),P1=Base+Along*(Side*130),Dir=-Along*Side;
   for(int32 J=0;J<3;++J){const FVector Start=P0+Dir*(35*J),End=P0+Dir*(35*(J+1));const float Top=369+(J+1)*15.75f;S::Step(Collision,TEXT("SD_Concrete"),Start,End,Out,48,361,Top);S::Step(Collision,TEXT("SD_Gold"),Start,Start+Dir*4,Out,46,Top,Top+.2f);}
   for(float Edge:{-1.f,1.f})Rail(P0+Out*(Edge*56),P1+Out*(Edge*56)+FVector(0,0,47.25f));
  }
  if(HouseStadiumDetail::Portal(K))
  {
   // A real passage from the unchanged apron (-57) to a369cm landing, then the row-D cross aisle.
   //25x17.04cm risers; the smallest perpendicular tread here is>35cm. No exterior stair leaves the existing apron.
   constexpr float R0=1.08f,R1=.882f,Bottom=-57.f,Landing=369.f;constexpr int32 Count=25;
   const FVector P0=S::Mid(K,R0,Bottom),P1=S::Mid(K,R1,Landing),Dir=(P1-P0).GetSafeNormal2D();const float Run=FVector::Dist2D(P0,P1)/Count;
   for(int32 J=0;J<Count;++J){const FVector Start=P0+Dir*(J*Run),End=P0+Dir*((J+1)*Run);const float Top=Bottom+(Landing-Bottom)*(J+1)/Count;S::Step(Collision,TEXT("SD_Concrete"),Start,End,Along,150,Bottom-8,Top);S::Step(Collision,TEXT("SD_Gold"),Start,Start+Dir*4,Along,148,Top,Top+.2f);}
   for(float Side:{-1.f,1.f})Rail(P0+Along*(Side*143),P1+Along*(Side*143));
   S::Step(Collision,TEXT("SD_Concrete"),S::Mid(K,.786f,Landing),S::Mid(K,.882f,Landing),Along,150,Bottom-8,Landing);
   // Closed underside and sidewalls through the upper tiers; the roof stays241cm above the highest stair.
   S::Step(Collision,TEXT("SD_Concrete"),S::Mid(K,.882f,628),S::Mid(K,1.f,628),Along,175,610,628);
   for(float Side:{-1.f,1.f})S::Step(Collision,TEXT("SD_Concrete"),S::Mid(K,.882f,0)+Along*(Side*168),S::Mid(K,1.f,0)+Along*(Side*168),Along,18,Bottom-8,628);
   // Stepped cheeks guard both the passage and each exposed seating edge. The row8 parapet is above the passage roof.
   for(int32 Row=4;Row<=7;++Row)for(float Side:{-1.f,1.f}){const float R=.69f+Row*.024f;S::Step(Collision,TEXT("SD_Concrete"),S::Mid(K,R,0)+Along*(Side*168),S::Mid(K,R+.024f,0)+Along*(Side*168),Along,18,Bottom-8,180+Row*63.f+95);}
   Rail(S::Mid(K,.882f,684)-Along*175,S::Mid(K,.882f,684)+Along*175);
   const FVector Outer=S::Mid(K,1.f,371);Box(TEXT("SD_Concrete"),Outer+Out*8,{372,24,22},Rot);
   const FVector Board=S::Mid(K,1.f,414)+Out*23;Box(TEXT("SD_Purple"),Board,{140,6,38},Rot);Letter(FString::Printf(TEXT("SEC %d"),K+1),Board+Out*3.2f,-Along,Out,17);
   const FVector Inner=S::Mid(K,.797f,539)+Along*232;Box(TEXT("SD_Purple"),Inner,{130,6,36},Rot);Letter(FString::Printf(TEXT("SEC %d"),K+1),Inner-Out*3.2f,Along,-Out,16);
   for(float Offset:{-46.f,46.f}){FVector Foot=Inner+Along*Offset;Foot.Z=369;Pipe(TEXT("SD_Steel"),Foot,Foot+FVector(0,0,188),2);}
   UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_PORTAL segment=%d risers=25 rise=17.04 wallWidth=300 railClear=280 landingZ=369 crossAisleRow=3 originalSTA=unchanged"),K);
  }
  if(HouseStadiumDetail::Aisle(K))
  {
   // Ground-to-front and top-to-rim links are real16.86/16.71cm steps; no63cm climb remains on this route.
   auto Link=[&](float R0,float R1,float Z0,float Z1,int32 Steps){FVector P0=S::Mid(K,R0,Z0),P1=S::Mid(K,R1,Z1);const FVector D=(P1-P0).GetSafeNormal2D();const float Run=FVector::Dist2D(P0,P1)/Steps;for(int32 J=0;J<Steps;++J){const float Top=FMath::Lerp(Z0,Z1,float(J+1)/Steps);const FVector Start=P0+D*(Run*J),End=P0+D*(Run*(J+1));S::Step(Collision,TEXT("SD_Concrete"),Start,End,Along,S::HalfAisle,Z0-8,Top);S::Step(Collision,TEXT("SD_Gold"),Start,Start+D*4.f,Along,S::HalfAisle-2,Top,Top+.2f);}Rail(P0,P1,false);UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_STAIR segment=%d count=%d rise=%.3f run=%.3f width=%.0f"),K,Steps,(Z1-Z0)/Steps,Run,2*S::HalfAisle);};
   Link(.60f,.69f,-56,180,14);Link(.954f,1.025f,873,990,7);
   for(float Side:{-1.f,1.f}){Rail(S::Mid(K,.954f,873)+Along*(Side*S::HalfAisle),S::Mid(K,.954f+.071f*5.f/7.f,873+117.f*5.f/7.f)+Along*(Side*S::HalfAisle));for(int32 J=0;J<5;++J){const float R0=.954f+.071f*J/7.f,R1=.954f+.071f*(J+1)/7.f,Top=873+117.f*(J+1)/7.f+95.f;S::Step(Collision,TEXT("SD_Concrete"),S::Mid(K,R0,0)+Along*(Side*(S::HalfAisle+6)),S::Mid(K,R1,0)+Along*(Side*(S::HalfAisle+6)),Along,6,865,Top);}}
   for(int32 Row=0;Row<11;++Row)Rail(S::Mid(K,.69f+Row*.024f,180+Row*63.f),S::Mid(K,.714f+Row*.024f,243+Row*63.f),false);
   // Section board is beside the stair, never at head height across its centre.
   const FVector Board=S::Mid(K,.985f,1140)+Along*(S::HalfAisle+55.f);Box(TEXT("SD_Purple"),Board,{105,6,38},Rot);Letter(FString::Printf(TEXT("SEC %d"),K/2+1),Board-Out*3.2f,Along,-Out,16.f);
   for(float Offset:{-37.f,37.f}){FVector Foot=Board+Along*Offset;Foot.Z=990;Pipe(TEXT("SD_Steel"),Foot,Foot+FVector(0,0,169),2);Box(TEXT("SD_Steel"),Foot+FVector(0,0,1),{10,10,2},Rot);}
  }
  // Drain channel/grate and outer-edge guard, clear of the press box and its exact tour.
  if(K<7||K>12){Rail(S::At(A,1.044f,990),S::At(B,1.044f,990));const FVector L=S::At(A,.998f,990.5f),R=S::At(B,.998f,990.5f);const int32 Count=FMath::Max(1,int(FVector::Dist2D(L,R)/40.f));for(int32 I=0;I<Count;++I){const FVector P=FMath::Lerp(L,R,float(I+.5f)/Count);if(HouseStadiumDetail::Aisle(K)&&FMath::Abs(float(I+.5f)/Count-.5f)*FVector::Dist2D(L,R)<S::HalfAisle+10)continue;Box(TEXT("SD_Dark"),P,{35,12,1},Rot,false,12000);for(float Offset:{-12.f,0.f,12.f})Box(TEXT("SD_Steel"),P+Along*Offset+FVector(0,0,.7f),{2,12,1},Rot,false,8000);}}
 }
 const FVector Cushion=S::Mid(25,.964f,180+11*63.f+47.6f);Box(TEXT("SD_Fabric"),Cushion,{38,32,5},FRotator(0,31,0));
 UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_SEATING plankSections=%d aisles=8 studentSegments=22..26 legacyTierRise=63 intermediateRise=15.75 newLights=0"),Planks);
}
