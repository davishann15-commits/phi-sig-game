#include "HouseStadiumDetail.h"
#include "HouseCampusFallKit.h"
#include "HouseBlackHallPaint.h"
namespace HouseStadiumDetailInteriorsImpl
{
FVector At(float Deg,float R,float Z){const float A=FMath::DegreesToRadians(Deg);return {2400+FMath::Cos(A)*5600*R,-13300+FMath::Sin(A)*3800*R,Z};}
void Box(FHallMesh& M,FName K,FVector C,FVector D,FRotator R=FRotator::ZeroRotator){M.Box(K,FTransform(R,C),D*.5f,FHallUV::World(100));}
// Small physical bevels keep padded/table/motor silhouettes from reading as cubes at1–2m.
void SoftBox(FHallMesh& M,FName Key,FVector Centre,FVector Dimensions,FRotator Rotation,float RequestedBevel)
{
 const float Bevel=FMath::Min(RequestedBevel,float(Dimensions.GetMin())*.2f),H[]={float(Dimensions.X*.5),float(Dimensions.Y*.5),float(Dimensions.Z*.5)};
 const FVector Axis[]={FVector::ForwardVector,FVector::RightVector,FVector::UpVector};const auto UV=FHallUV::World(100);
 auto P=[&](FVector V){return Centre+Rotation.RotateVector(V);};auto N=[&](FVector V){return Rotation.RotateVector(V);};
 for(int32 A=0;A<3;++A)for(float S:{-1.f,1.f}){const int32 B=(A+1)%3,C=(A+2)%3;const FVector At=Axis[A]*(S*H[A]),U=Axis[B]*(H[B]-Bevel),V=Axis[C]*(H[C]-Bevel);M.Quad(Key,P(At-U-V),P(At+U-V),P(At+U+V),P(At-U+V),N(Axis[A]*S),UV);}
 for(int32 A=0;A<2;++A)for(int32 B=A+1;B<3;++B)for(float SA:{-1.f,1.f})for(float SB:{-1.f,1.f}){const int32 C=3-A-B;const FVector U=Axis[A]*(SA*H[A])+Axis[B]*(SB*(H[B]-Bevel)),V=Axis[A]*(SA*(H[A]-Bevel))+Axis[B]*(SB*H[B]),End=Axis[C]*(H[C]-Bevel);M.Quad(Key,P(U-End),P(V-End),P(V+End),P(U+End),N((Axis[A]*SA+Axis[B]*SB).GetSafeNormal()),UV);}
 for(float X:{-1.f,1.f})for(float Y:{-1.f,1.f})for(float Z:{-1.f,1.f})HouseCampusFallKit::Tri(M,Key,P({X*H[0],Y*(H[1]-Bevel),Z*(H[2]-Bevel)}),P({X*(H[0]-Bevel),Y*H[1],Z*(H[2]-Bevel)}),P({X*(H[0]-Bevel),Y*(H[1]-Bevel),Z*H[2]}),N(FVector(X,Y,Z).GetSafeNormal()),UV);
}
void Word(FHallMesh& M,FString Text,FVector C,FVector Right,FVector N,float Height)
{const float Pitch=Height*.82f;const FVector Left=C-Right*((Text.Len()-1)*Pitch*.5f+Height/3.f);for(int32 I=0;I<Text.Len();++I)for(const auto& Stroke:HallGlyph(Text[I]))for(int32 J=1;J<Stroke.Num();++J){auto P=[&](FVector2D Q){return Left+Right*(I*Pitch+Q.X*Height/6.f)+FVector(0,0,Q.Y*Height/6.f);};HouseCampusFallKit::Bar(M,TEXT("SD_Cream"),P(Stroke[J-1]),P(Stroke[J]),Height*.08f,.15f,N);}}
}
void HouseStadiumDetail::WoodLockers(FHallMesh& Solid,FHallMesh& Detail,FVector Origin,FVector Along,int32 Count,FVector Facing)
{
 namespace I=HouseStadiumDetailInteriorsImpl;const FRotator R=Along.Rotation();const TCHAR* Names[]={TEXT("WARD"),TEXT("BELL"),TEXT("REED"),TEXT("FINCH"),TEXT("MASON"),TEXT("GREER"),TEXT("COLE"),TEXT("ASH"),TEXT("HOLT"),TEXT("DRAKE")};
 for(int32 L=0;L<Count;++L)
 {
  const FVector C=Origin+Along*(L*38+19)+Facing*23;
  I::Box(Solid,TEXT("SD_Wood"),C+FVector(0,0,4),{37,46,8},R);I::Box(Solid,TEXT("SD_Wood"),C+FVector(0,0,182),{37,46,8},R);
  for(float Side:{-1.f,1.f})I::Box(Solid,TEXT("SD_Wood"),C+Along*(Side*18)+FVector(0,0,94),{2,46,174},R);
  I::Box(Solid,TEXT("SD_Wood"),C-Facing*22+FVector(0,0,94),{36,2,174},R);I::Box(Solid,TEXT("SD_Wood"),C+FVector(0,0,152),{36,46,3},R);
  I::Box(Detail,TEXT("SD_Steel"),C+Facing*23+FVector(0,0,171),{30,1,10},R);I::Word(Detail,Names[L%10],C+Facing*23.6f+FVector(0,0,168),Along,Facing,3.6f);
  Detail.Cylinder(TEXT("SD_Steel"),C-Facing*17+FVector(0,0,130),C-Facing*10+FVector(0,0,130),1.5f,8,FHallUV::World(100));
 }
}
void HouseStadiumDetail::Interiors(FHallMesh& Solid,FHallMesh& Clutter,FHallMesh&)
{
 if(!Enabled()||Step()<6)return;namespace I=HouseStadiumDetailInteriorsImpl;
 constexpr float Floor=-55.f;
 // Trainer's table by the east end wall, outside the existing64.3/58.5degree route.
 const FVector Table=I::At(51.8f,.823f,Floor);const FRotator Rot(0,141.8f,0);const FVector X=Rot.Vector(),Y(-X.Y,X.X,0);
 I::SoftBox(Solid,TEXT("SD_Wood"),Table+FVector(0,0,68),{183,64,10},Rot,.8f);I::SoftBox(Solid,TEXT("SD_Padding"),Table+FVector(0,0,75),{185,66,6},Rot,1.2f);
 for(float A:{-76.f,76.f})for(float B:{-23.f,23.f})I::Box(Solid,TEXT("SD_Wood"),Table+X*A+Y*B+FVector(0,0,32.5f),{7,7,65},Rot);
 for(int32 J=0;J<3;++J){const FVector P=Table+X*(J*17-35)+FVector(0,0,78);const TArray<FVector2D> Roll={{3,0},{3,6},{7,6},{7,0},{3,0}};HouseCampusFallKit::Lathe(Clutter,TEXT("SD_Cream"),P,Roll,24);}
 // A compact oval steel whirlpool: real hollow walls, rolled rim and a small period agitator.
 // The64.3degree door-to-room line stays clear; leave room to walk behind the tub/locker bench too.
 const FVector Tub=I::At(55.6f,.83f,Floor);const FRotator TR(0,145.6f,0);
 const FVector TX=TR.Vector(),TY(-TX.Y,TX.X,0);
 auto Oval=[&](float A,float RX,float RY,float Z){return Tub+TX*(FMath::Cos(A)*RX)+TY*(FMath::Sin(A)*RY)+FVector(0,0,Z);};
 const auto MetalUV=FHallUV::World(100);
 for(int32 J=0;J<48;++J){const float A=J*2*PI/48,B=(J+1)*2*PI/48;const FVector Out=(TX*(FMath::Cos((A+B)*.5f)/62.f)+TY*(FMath::Sin((A+B)*.5f)/36.f)).GetSafeNormal();
  Solid.Quad(TEXT("SD_Aluminium"),Oval(A,62,36,5),Oval(B,62,36,5),Oval(B,62,36,76),Oval(A,62,36,76),Out,MetalUV);
  Solid.Quad(TEXT("SD_Aluminium"),Oval(A,58,32,8),Oval(A,58,32,76),Oval(B,58,32,76),Oval(B,58,32,8),-Out,MetalUV);
  Solid.Quad(TEXT("SD_Aluminium"),Oval(A,62,36,76),Oval(B,62,36,76),Oval(B,58,32,76),Oval(A,58,32,76),FVector::UpVector,MetalUV);
  HouseCampusFallKit::Tri(Solid,TEXT("SD_Steel"),Tub+FVector(0,0,8),Oval(A,58,32,8),Oval(B,58,32,8),FVector::UpVector,MetalUV);
  HouseCampusFallKit::Tri(Clutter,TEXT("SD_Wet"),Tub+FVector(0,0,26),Oval(A,58,32,26),Oval(B,58,32,26),FVector::UpVector,MetalUV);
  Clutter.Cylinder(TEXT("SD_Aluminium"),Oval(A,60,34,78),Oval(B,60,34,78),2.2f,6,MetalUV);
 }
 for(float A:{-44.f,44.f})for(float B:{-21.f,21.f})I::SoftBox(Solid,TEXT("SD_Steel"),Tub+TX*A+TY*B+FVector(0,0,3),{8,8,6},TR,.5f);
 const FVector Motor=Tub-TX*43;I::SoftBox(Solid,TEXT("SD_Cream"),Motor+FVector(0,0,102),{24,27,34},TR,1.5f);Clutter.Cylinder(TEXT("SD_Steel"),Motor+FVector(0,0,28),Motor+FVector(0,0,89),3.f,8,MetalUV);
 I::Box(Clutter,TEXT("SD_Dark"),Motor+TY*14+FVector(0,0,107),{9,1,7},TR);
 const FVector Spill=I::At(55.3f,.809f,Floor+.10f);TArray<FVector> WetEdge;FRandomStream WetSeed(861018);for(int32 J=0;J<20;++J){const float A=J*2*PI/20,Scale=WetSeed.FRandRange(.78f,1.18f);WetEdge.Add(Spill+TX*(FMath::Cos(A)*49*Scale)+TY*(FMath::Sin(A)*33*Scale));}HouseCampusFallKit::Fan(Clutter,TEXT("SD_Wet"),Spill,WetEdge,FVector::UpVector);
 // A second small play diagram on the existing locker-room end wall, no room or doorway added.
 const FVector Across=(I::At(50,.86f,0)-I::At(50,.84f,0)).GetSafeNormal(),Out(-Across.Y,Across.X,0),Board=I::At(50.f,.855f,70)+Out*25;
 I::Box(Solid,TEXT("SD_Wood"),Board,{145,8,87},Across.Rotation());HouseCampusFallKit::Card(Clutter,TEXT("SD_Play"),Board+Out*4.2f,Across,FVector::UpVector,135,78,Out);
 // New pads line existing tunnel cheeks only. Opening stays300cm wide;4cm pads leave292cm.
 const FVector MouthA=I::At(54,.695f,Floor),MouthB=I::At(63,.695f,Floor),AcrossTunnel=(MouthB-MouthA).GetSafeNormal(),Rad(AcrossTunnel.Y,-AcrossTunnel.X,0),Mouth=(MouthA+MouthB)*.5f;
 for(float Side:{-1.f,1.f}){I::SoftBox(Solid,TEXT("SD_Padding"),Mouth+AcrossTunnel*(Side*148)+Rad*15+FVector(0,0,105),{90,4,205},Rad.Rotation(),.8f);for(float Z:{35.f,105.f,175.f})Clutter.Cylinder(TEXT("SD_Steel"),Mouth+AcrossTunnel*(Side*145.8f)+Rad*12+FVector(0,0,Z),Mouth+AcrossTunnel*(Side*145.4f)+Rad*12+FVector(0,0,Z),1.2f,6,FHallUV::World(100));}
 const FVector Torn=I::At(58.5f,.715f,Floor+.12f);
 // A real irregular torn edge, mapped to the authored paper artwork; no black rectangle faking a hole.
 const TArray<FVector2D> Edge={{-95,-23.5f},{90,-23.5f},{85,-15},{95,-6},{83,0},{93,6},{87,14},{95,23.5f},{-95,23.5f}};
 FHallSection& Paper=Clutter.Sec(TEXT("SD_Beat"));for(int32 J=0;J<Edge.Num();++J){const int32 Base=Paper.V.Num();for(FVector2D Q:{FVector2D::ZeroVector,Edge[J],Edge[(J+1)%Edge.Num()]}){Paper.V.Add(Torn+AcrossTunnel*Q.X+Rad*Q.Y);Paper.N.Add(FVector::UpVector);Paper.UV.Add({Q.X/190.f+.5f,.5f-Q.Y/47.f});Paper.T.Add(FProcMeshTangent(AcrossTunnel,false));}if(FVector::CrossProduct(Paper.V[Base+1]-Paper.V[Base],Paper.V[Base+2]-Paper.V[Base]).Z<0)Paper.I.Append({Base,Base+1,Base+2});else Paper.I.Append({Base,Base+2,Base+1});}
 UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_INTERIOR sameDoors=1 sameRoute=1 trainer=1 whirlpool=1 tunnelPads=2 newRooms=0 newLights=0"));
}
