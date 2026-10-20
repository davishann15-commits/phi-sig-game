#include "HouseStadiumDetail.h"
#include "HouseCampusPolish.h"
#include "HouseCampusFallKit.h"
namespace HouseStadiumDetailStructuresImpl
{
FVector Bowl(float Degrees,float R,float Z){const float A=FMath::DegreesToRadians(Degrees);return {2400+5600*FMath::Cos(A)*R,-13300+3800*FMath::Sin(A)*R,Z};}
void Horn(FHallMesh& M,FVector C)
{
 const auto UV=FHallUV::World(80);const FVector N(0,-1,0);
 const FVector Mouth[]={C+FVector(-27,-31,-18),C+FVector(27,-31,-18),C+FVector(27,-31,18),C+FVector(-27,-31,18)};
 const FVector Throat[]={C+FVector(-7,22,-6),C+FVector(7,22,-6),C+FVector(7,22,6),C+FVector(-7,22,6)};
 for(int32 J=0;J<4;++J){const int32 K=(J+1)%4;const FVector Normal=FVector::CrossProduct(Mouth[K]-Mouth[J],Throat[J]-Mouth[J]).GetSafeNormal();M.Quad(TEXT("SD_Steel"),Mouth[J],Mouth[K],Throat[K],Throat[J],Normal,UV);M.Quad(TEXT("SD_Steel"),Throat[J],Throat[K],Mouth[K],Mouth[J],-Normal,UV);M.Cylinder(TEXT("SD_Aluminium"),Mouth[J],Mouth[K],.8f,6,UV);}
 M.Quad(TEXT("SD_Dark"),Throat[0]+N*.1f,Throat[1]+N*.1f,Throat[2]+N*.1f,Throat[3]+N*.1f,N,UV);
}
}
void AHouseStadiumDetail::Structures()
{
 // Lower press row, exact stair tower/doors/floors and original roof stay. A shallow second coaching deck is above it.
 const float Front=-10006.f;const FVector N(0,-1,0),Right(-1,0,0);
 for(float X=170;X<4650;X+=88)
 {
  Box(TEXT("SD_Purple"),{X,Front,1635},{86,8,285});for(float Z:{1510.f,1760.f})Box(TEXT("SD_Rust"),{X-30,Front-4.3f,Z},{1.6f,.4f,18},FRotator::ZeroRotator,false,8000);
 }
 // Angled outward glazing reveals recessed desk/radio silhouettes, mullions and a deep sill rather than a flat sign.
 for(float X=260;X<4500;X+=300)
 {
  const FVector Bottom(X,Front-18,1190),Top(X,Front-50,1425);Unique.Quad(TEXT("SD_Glass"),Bottom+FVector(-137,0,0),Bottom+FVector(137,0,0),Top+FVector(137,0,0),Top+FVector(-137,0,0),N,FHallUV::World(180));
  Pipe(TEXT("SD_Aluminium"),Bottom+FVector(-140,0,0),Top+FVector(-140,0,0),3.5f);Box(TEXT("SD_Aluminium"),{X,Front-34,1190},{285,48,6});
  // The original STA desks, phones and typewriters stay in this lower room; no duplicate equipment floats over them.
 }
 Letter(TEXT("CALDER STATE"),{2400,Front-4.1f,1630},Right,N,145,TEXT("SD_Cream"));
 // Street-facing painted siding band: above the retained room ceiling, leaving its original glazing/routes intact.
 for(float X=170;X<4650;X+=88){Box(TEXT("SD_Purple"),{X,-9221,1635},{86,6,285});for(float Z:{1510.f,1760.f})Box(TEXT("SD_Rust"),{X+26,-9217.7f,Z},{1.6f,.4f,18},FRotator::ZeroRotator,false,8000);}
 Letter(TEXT("CALDER STATE"),{2400,-9217.8f,1630},{1,0,0},{0,1,0},145,TEXT("SD_Cream"));
 // Compact upper row leaves both original antenna installations on their original lower-roof wings.
 Box(TEXT("SD_Purple"),{2200,-9795,1968},{2900,250,96});Box(TEXT("SD_Purple"),{2200,-9795,2336},{2900,250,64});Box(TEXT("SD_Purple"),{2200,-9895,2336},{2900,60,64});
 Box(TEXT("SD_Purple"),{2200,-9668,2160},{2900,6,288});for(float X:{750.f,3650.f})Box(TEXT("SD_Purple"),{X,-9790,2160},{10,240,288});Box(TEXT("SD_Steel"),{2200,-9795,2368},{3100,260,16});
 Box(TEXT("SD_Purple"),{2200,-9885,2023},{2900,50,14});
 for(const FVector Corner:{FVector(763.5f,-9897,2165),FVector(3626.5f,-9897,2165)})Box(TEXT("SD_Purple"),Corner,{Corner.X<1000?27.f:47.f,60,270});
 // Seven actual recesses (dark backing/glass) and supported radio/coach hardware; no lit fixtures added.
 for(int32 I=0;I<7;++I)
 {
  const float X=990+I*400.f;Box(TEXT("SD_Dark"),{X,-9672,2165},{380,4,260},FRotator::ZeroRotator,false,30000);
  if(I==0)Box(TEXT("SD_Aluminium"),{X-200,-9897,2165},{26,14,272},FRotator(0,0,5.214f));Box(TEXT("SD_Aluminium"),{X+200,-9897,2165},{26,14,272},FRotator(0,0,5.214f));
  Box(TEXT("SD_Aluminium"),{X,-9885,2030},{390,18,7});Box(TEXT("SD_Aluminium"),{X,-9909,2300},{390,18,7});
  Box(TEXT("SD_Glass"),{X,-9897,2165.5f},{374,1.2f,264.1f},FRotator(0,0,5.214f));
  Box(TEXT("SD_Wood"),{X,-9800,2094},{380,80,4});for(float A:{-150.f,150.f})for(float B:{-28.f,28.f})Box(TEXT("SD_Steel"),{X+A,-9800+B,2055},{4,4,78});
  Box(TEXT("SD_Steel"),{X-68,-9780,2116.5f},{63,35,41});Pipe(TEXT("SD_Dark"),{X+58,-9780,2096},{X+60,-9780,2186},2,false,20000);Box(TEXT("SD_Dark"),{X+77,-9785,2186},{35,8,12},FRotator(0,0,12),false,20000);
 }
 // Roof camera platform, guardrails, toe boards, two unstaffed tripods, access ladder and PA horns.
 const float Roof=2380;
 for(int32 I=0;I<9;++I)Box(TEXT("SD_Steel"),{902+I*324.f,-9792.5f,Roof},{323,185,9});
 Rail({740,-9885,Roof},{3660,-9885,Roof});Rail({740,-9700,Roof},{3660,-9700,Roof});Rail({740,-9885,Roof},{740,-9865,Roof});Rail({740,-9775,Roof},{740,-9700,Roof});Rail({3660,-9885,Roof},{3660,-9700,Roof});
 // The ladder reaches a side landing, outside the solid roof edge; its access guard is left closed after the game.
 Box(TEXT("SD_Steel"),{695,-9820,Roof},{90,90,9});Rail({650,-9865,Roof},{740,-9865,Roof});Rail({650,-9775,Roof},{740,-9775,Roof});Rail({650,-9865,Roof},{650,-9775,Roof});
 for(float X:{1650.f,3070.f}){const FVector C(X,-9790,Roof);for(int32 J=0;J<3;++J){const float A=J*2*PI/3;Pipe(TEXT("SD_Steel"),C+FVector(FMath::Cos(A)*42,FMath::Sin(A)*42,0),C+FVector(0,0,130),2.6f);}Box(TEXT("SD_Dark"),C+FVector(0,0,143),{80,32,30});Pipe(TEXT("SD_Dark"),C+FVector(0,-16,146),C+FVector(0,-45,146),12);}
 for(float X:{600.f,650.f})Pipe(TEXT("SD_Steel"),{X,-9820,1920},{X,-9820,Roof+120},3.f);
 for(float Z=1950;Z<Roof;Z+=30)Pipe(TEXT("SD_Steel"),{600,-9820,Z},{650,-9820,Z},2.1f);
 for(int32 H=0;H<4;++H){const FVector C(2500+(H%2)*95,-9920,2280+(H/2)*60);HouseStadiumDetailStructuresImpl::Horn(Unique,C);Box(TEXT("SD_Steel"),C+FVector(0,29,0),{18,20,16});}
 for(float X:{700.f,3700.f}){Box(TEXT("SD_Steel"),{X,-9730,2378},{24,24,4});Pipe(TEXT("SD_Aluminium"),{X,-9730,Roof},{X,-9730,Roof+570},4);Pipe(TEXT("SD_Cream"),{X+7,-9730,Roof+20},{X+7,-9730,Roof+553},.45f,false,20000);}
 // Incandescent scoreboard stays on its existing footprint. Static bulbs are HISM instances with a few failed cells.
 const float Y=-16966.f;Box(TEXT("SD_Purple"),{2400,Y-6,1700},{1552,10,778});
 auto Words=[&](FString Text,float X,float Z,float Height){Letter(Text,{X,Y-.9f,Z},{1,0,0},{0,1,0},Height,TEXT("SD_Cream"));};
 Words(TEXT("HOME"),1925,2045,53);Words(TEXT("GUESTS"),2875,2045,53);
 Words(TEXT("GEESE"),1925,1970,32);Words(TEXT("DUNMORE"),2875,1970,32);
 int32 Index=0;auto Bulbs=[&](FString Text,float CentreX,float Top,float Pitch){const float Left=CentreX-(Text.Len()*6-1)*Pitch*.5f;
  for(int32 I=0;I<Text.Len();++I){const uint8* Rows=HouseCampusFallKit::BulbGlyph(Text[I]);for(int32 R=0;R<7;++R)for(int32 C=0;C<5;++C){const bool Lit=((Rows[R]>>(4-C))&1)!=0;FName K=Lit?FName(TEXT("SD_Bulb")):FName(TEXT("SD_DeadBulb"));if(Lit&&Index%137==19)K=TEXT("SD_DeadBulb");else if(Lit&&Index%211==22)K=TEXT("SD_FlickerBulb");const FVector P(Left+(I*6+C+.5f)*Pitch,Y+3,Top-(R+.5f)*Pitch);Pipe(K,P,P+FVector(0,2,0),Pitch*.31f,false,40000);++Index;}}};
 Bulbs(TEXT("24"),1925,1910,25);Bulbs(TEXT("17"),2875,1910,25);Bulbs(TEXT("00:00"),2400,1878,11);
 const TCHAR* Labels[]={TEXT("QTR"),TEXT("DOWN"),TEXT("TO GO"),TEXT("BALL ON")};const TCHAR* Values[]={TEXT("4"),TEXT("4"),TEXT("07"),TEXT("32")};
 for(int32 I=0;I<4;++I){const float X=1805+I*400.f;Words(Labels[I],X,1610,29);Bulbs(Values[I],X,1560,13);}
 Words(TEXT("TIME OUTS"),2400,1395,27);Bulbs(TEXT("1"),2030,1442,8);Bulbs(TEXT("0"),2770,1442,8);
 for(float Z:{1323.f,2090.f})Box(TEXT("SD_Gold"),{2400,Y+4,Z},{1555,7,9});for(float X:{1623.f,3177.f})Box(TEXT("SD_Gold"),{X,Y+4,1705},{9,7,774});
 const FName Sponsors[]={TEXT("SD_SponsorHardware"),TEXT("SD_SponsorDiner"),TEXT("SD_SponsorBank")};for(int32 I=0;I<3;++I){const float X=1870+I*530.f;Box(TEXT("SD_Steel"),{X,Y-4,1170},{520,16,190});Sign(Sponsors[I],{X,Y+5,1170},{1,0,0},{0,1,0},510,180);}
 // Goose seal, using the existing invented homecoming artwork (never a real school mark).
 Sign(TEXT("SD_Goose"),{2400,Y+6,2005},{1,0,0},{0,1,0},120,100);
 // Back framing, rungs and grated access deck are visible at oblique angles.
 for(float X:{1740.f,3060.f})Box(TEXT("SD_Steel"),{X,Y-100,1000},{28,32,2200});for(float Z=100;Z<1900;Z+=280){Pipe(TEXT("SD_Steel"),{1740,Y-120,Z},{3060,Y-120,Z+280},5);Pipe(TEXT("SD_Steel"),{3060,Y-135,Z},{1740,Y-135,Z+280},5);}
 Box(TEXT("SD_Steel"),{2400,Y-200,1280},{1580,170,12});Rail({1610,Y-280,1286},{3190,Y-280,1286});Rail({1610,Y-115,1286},{3190,Y-115,1286});Rail({1610,Y-280,1286},{1610,Y-112,1286});Rail({3190,Y-280,1286},{3190,Y-112,1286});
 for(float X:{3065.f,3110.f})Pipe(TEXT("SD_Steel"),{X,Y-325,-55},{X,Y-325,1400},3);for(float Z=-25;Z<1400;Z+=30)Pipe(TEXT("SD_Steel"),{3065,Y-325,Z},{3110,Y-325,Z},2);
}
void AHouseStadiumDetail::Masts()
{
 namespace S=HouseStadiumDetailStructuresImpl;int32 Mast=0;
 for(float Deg:{35.f,90.f,145.f})
 {
  const FVector Base=S::Bowl(Deg,1.04f,-60);const bool Middle=Deg==90.f;const float Bottom=Middle?1980.f:260.f;
  // Existing east-side route passes close to a mast's foot. Keep its original20cm core clear below head height.
  if(!Middle)for(int32 Side=0;Side<4;++Side){const float A=Side*HALF_PI+PI*.25f;Pipe(TEXT("SD_Steel"),Base+FVector(0,0,160),Base+FVector(FMath::Cos(A)*113,FMath::Sin(A)*113,500),4.f);}
  // Fixed mast core/fixture bank/catwalk from CampusPolish remain; lattice tapers around that same centre.
  for(float Z=Bottom;Z<3780;Z+=240)
  {
   const float Width=FMath::Lerp(130.f,46.f,Z/3900.f),W1=FMath::Lerp(130.f,46.f,(Z+240)/3900.f);
   for(int32 Side=0;Side<4;++Side){const float A=Side*HALF_PI+PI*.25f,B=A+HALF_PI;const FVector P=Base+FVector(FMath::Cos(A)*Width,FMath::Sin(A)*Width,Z),Q=Base+FVector(FMath::Cos(A)*W1,FMath::Sin(A)*W1,Z+240),R=Base+FVector(FMath::Cos(B)*Width,FMath::Sin(B)*Width,Z),T=Base+FVector(FMath::Cos(B)*W1,FMath::Sin(B)*W1,Z+240);Pipe(TEXT("SD_Steel"),P,Q,4.3f);Pipe(TEXT("SD_Steel"),P,R,2.8f);Pipe(TEXT("SD_Steel"),P,T,2.1f);Pipe(TEXT("SD_Steel"),R,Q,2.1f);}
  }++Mast;
 }
 UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_MASTS fixedCentres=%d newLights=0 retainedCatwalks=3 retainedCagedLadders=3 lowerCoreClearanceCm=260"),Mast);
}
