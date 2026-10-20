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
 const FVector Axis[]={FVector::ForwardVector,FVector::RightVector,FVector::UpVector};FHallUV UV=FHallUV::World(100);
 const bool AgedWood=HouseStadiumDetail::Step()>=7&&(Key.ToString().StartsWith(TEXT("SD_LockerOak"))||Key==TEXT("SD_BenchWood")||Key.ToString().StartsWith(TEXT("SD_FurnitureOak")));
 const FName BevelKey=Key.ToString().StartsWith(TEXT("SD_FurnitureOak"))?FName(TEXT("SD_FurnitureOakRubbed")):AgedWood?FName(TEXT("SD_WoodEdge")):Key;
 if(AgedWood){UV=FHallUV::Local(FTransform(Rotation,Centre),100);UV.Offset={FMath::Fmod(FMath::Abs(float(Centre.X))*.013f,1.f),FMath::Fmod(FMath::Abs(float(Centre.Y))*.017f,1.f)};}
 if(Key==TEXT("SD_CoachUpholstery"))UV=FHallUV::Local(FTransform(Rotation,Centre),40);
 auto P=[&](FVector V){return Centre+Rotation.RotateVector(V);};auto N=[&](FVector V){return Rotation.RotateVector(V);};
 for(int32 A=0;A<3;++A)for(float S:{-1.f,1.f}){const int32 B=(A+1)%3,C=(A+2)%3;const FVector At=Axis[A]*(S*H[A]),U=Axis[B]*(H[B]-Bevel),V=Axis[C]*(H[C]-Bevel);M.Quad(Key,P(At-U-V),P(At+U-V),P(At+U+V),P(At-U+V),N(Axis[A]*S),UV);}
 for(int32 A=0;A<2;++A)for(int32 B=A+1;B<3;++B)for(float SA:{-1.f,1.f})for(float SB:{-1.f,1.f}){const int32 C=3-A-B;const FVector U=Axis[A]*(SA*H[A])+Axis[B]*(SB*(H[B]-Bevel)),V=Axis[A]*(SA*(H[A]-Bevel))+Axis[B]*(SB*H[B]),End=Axis[C]*(H[C]-Bevel);M.Quad(BevelKey,P(U-End),P(V-End),P(V+End),P(U+End),N((Axis[A]*SA+Axis[B]*SB).GetSafeNormal()),UV);}
 for(float X:{-1.f,1.f})for(float Y:{-1.f,1.f})for(float Z:{-1.f,1.f})HouseCampusFallKit::Tri(M,BevelKey,P({X*H[0],Y*(H[1]-Bevel),Z*(H[2]-Bevel)}),P({X*(H[0]-Bevel),Y*H[1],Z*(H[2]-Bevel)}),P({X*(H[0]-Bevel),Y*(H[1]-Bevel),Z*H[2]}),N(FVector(X,Y,Z).GetSafeNormal()),UV);
}
// Office-only soft upholstery and moulded telephone details; merged into the existing STA room meshes.
void Cushion(FHallMesh& Mesh,FHallMesh& Detail,FVector Centre,FVector Across,FVector Rise,FVector Facing,float Width,float Height,float Thickness,bool Back)
{
 const FName Key(TEXT("SD_CoachUpholstery"));const float A=Width*.5f,B=Height*.5f;
 auto Signed=[](float V){return FMath::Sign(V)*FMath::Pow(FMath::Abs(V),.48f);};
 auto Point=[&](float Radius,float Angle,float Side){
  const float U=A*Radius*Signed(FMath::Cos(Angle)),V=B*Radius*Signed(FMath::Sin(Angle)),Envelope=FMath::Max(0.f,1-Radius*Radius);
  const float Sag=Side>0?(Back?1.2f:2.3f)*FMath::Exp(-FMath::Square((U+2)/12.f)-FMath::Square((V-1)/11.f))*Envelope:0;
  const float Crease=Side>0?.12f*FMath::Sin(Angle*17+Radius*7)*FMath::Exp(-FMath::Square((Radius-.84f)/.12f))*Envelope:0;
  const float Bend=Back?1.5f*FMath::Square(U/A):-.35f*V/B;
  return Centre+Across*U+Rise*V+Facing*(Side*Thickness*.5f*FMath::Pow(Envelope,.64f)-Sag+Crease+Bend);
 };
 auto Normal=[&](float Radius,float Angle,float Side){if(Radius<.001f)return Facing*Side;const FVector T=Point(Radius,Angle+.003f,Side)-Point(Radius,Angle-.003f,Side),D=Point(FMath::Min(.9999f,Radius+.002f),Angle,Side)-Point(FMath::Max(.0001f,Radius-.002f),Angle,Side);FVector N=FVector::CrossProduct(T,D).GetSafeNormal();if(FVector::DotProduct(N,Facing)*Side<0)N=-N;return N;};
 constexpr int32 Rings=9,Segments=48;
 for(float Side:{-1.f,1.f})for(int32 J=0;J<Rings;++J)for(int32 K=0;K<Segments;++K){
  const float R0=J/float(Rings),R1=(J+1)/float(Rings),T0=K*2*PI/Segments,T1=(K+1)*2*PI/Segments;
  const FVector V[]={Point(R0,T0,Side),Point(R1,T0,Side),Point(R1,T1,Side),Point(R0,T1,Side)};
  const FVector N[]={Normal(R0,T0,Side),Normal(R1,T0,Side),Normal(R1,T1,Side),Normal(R0,T1,Side)};
  auto UV=[&](FVector P){return FVector2D(.5f+FVector::DotProduct(P-Centre,Across)/40.f,.5f-FVector::DotProduct(P-Centre,Rise)/40.f);};
  auto& Section=Mesh.Sec(Key);const int32 Start=Section.V.Num();
  if(J==0){HouseCampusFallKit::Tri(Mesh,Key,V[0],V[1],V[2],(N[0]+N[1]+N[2]).GetSafeNormal());for(int32 I=0;I<3;++I){Section.N[Start+I]=N[I];Section.UV[Start+I]=UV(V[I]);Section.T[Start+I]=FProcMeshTangent(Across,false);}}
  else{Mesh.QuadUV(Key,V[0],V[1],V[2],V[3],(N[0]+N[1]+N[2]+N[3]).GetSafeNormal(),UV(V[0]),UV(V[1]),UV(V[2]),UV(V[3]),Across);for(int32 I=0;I<4;++I)Section.N[Start+I]=N[I];}
 }
 for(int32 K=0;K<Segments;++K)Detail.Cylinder(TEXT("SD_PaddingDark"),Point(1,K*2*PI/Segments,1),Point(1,(K+1)*2*PI/Segments,1),.14f,6,FHallUV::World(100));
 // Fine, broken compression creases follow the curved surface rather than float over a flat patch.
 if(Width>30)for(int32 K=0;K<12;++K){const float Angle=.35f+K*.52f;for(int32 J=0;J<4;++J){const float R0=.79f+J*.035f,R1=R0+.025f;Detail.Cylinder(TEXT("SD_LeatherCrack"),Point(R0,Angle,1)+Normal(R0,Angle,1)*.025f,Point(R1,Angle+.012f,1)+Normal(R1,Angle+.012f,1)*.025f,.045f,4,FHallUV::World(100));}}
}
void DiscCard(FHallMesh& Mesh,FName Key,FVector Centre,FVector Across,FVector Rise,float Radius,int32 Cell)
{
 const FVector N=FVector::CrossProduct(Across,Rise).GetSafeNormal();
 for(int32 K=0;K<64;++K){const float T0=K*2*PI/64,T1=(K+1)*2*PI/64;const FVector V[]={Centre,Centre+(Across*FMath::Cos(T0)+Rise*FMath::Sin(T0))*Radius,Centre+(Across*FMath::Cos(T1)+Rise*FMath::Sin(T1))*Radius};auto& S=Mesh.Sec(Key);const int32 Start=S.V.Num();HouseCampusFallKit::Tri(Mesh,Key,V[0],V[1],V[2],N);for(int32 J=0;J<3;++J){S.UV[Start+J]={(Cell%2)*.5f+.25f+FVector::DotProduct(V[J]-Centre,Across)/Radius*.25f,(Cell/2)*.5f+.25f-FVector::DotProduct(V[J]-Centre,Rise)/Radius*.25f};S.T[Start+J]=FProcMeshTangent(Across,false);}}
}
void RotaryPhone(FHallMesh& Solid,FHallMesh& Detail,FVector Base,FVector X,FVector Y)
{
 const FVector Z=FVector::UpVector;const FName Body(TEXT("SD_PhoneBakelite"));auto P=[&](float U,float V,float H){return Base+X*U+Y*V+Z*H;};
 // Rounded moulded shell rises behind the dial; each cross-section has genuine curved corners.
 struct FRing{float X,Y,R,Z,Slope;};const FRing Rings[]={{12.0f,12.4f,3.2f,.3f,0},{12.6f,12.8f,3.7f,1.2f,0},{12.5f,12.5f,4,3.0f,.04f},{11.7f,11.4f,4.2f,5.8f,.16f},{10.6f,10.1f,4.2f,6.4f,.16f}};
 auto Rim=[&](int32 Ring,int32 Index){const auto& R=Rings[Ring];const int32 Corner=Index/8,Step=Index%8;const float Angle=(Corner*90.f+Step*90.f/7.f)*PI/180.f;const float U=(Corner==0||Corner==3?1.f:-1.f)*(R.X-R.R)+FMath::Cos(Angle)*R.R,V=(Corner<2?1.f:-1.f)*(R.Y-R.R)+FMath::Sin(Angle)*R.R;return P(U,V,R.Z+V*R.Slope);};
 for(int32 Ring=0;Ring<4;++Ring)for(int32 K=0;K<32;++K){const FVector V[]={Rim(Ring,K),Rim(Ring,(K+1)%32),Rim(Ring+1,(K+1)%32),Rim(Ring+1,K)};Solid.Quad(Body,V[0],V[1],V[2],V[3],FVector::CrossProduct(V[1]-V[0],V[3]-V[0]).GetSafeNormal(),FHallUV::World(30));auto& S=Solid.Sec(Body);const int32 First=S.N.Num()-4;auto N=[&](int32 R,int32 I){const FVector T=Rim(R,(I+1)%32)-Rim(R,(I+31)%32),D=Rim(FMath::Min(4,R+1),I)-Rim(FMath::Max(0,R-1),I);return FVector::CrossProduct(T,D).GetSafeNormal();};S.N[First]=N(Ring,K);S.N[First+1]=N(Ring,(K+1)%32);S.N[First+2]=N(Ring+1,(K+1)%32);S.N[First+3]=N(Ring+1,K);}
 TArray<FVector> Top,Bottom;for(int32 K=0;K<32;++K){Top.Add(Rim(4,K));Bottom.Add(Rim(0,K));}HouseCampusFallKit::Fan(Solid,Body,P(0,0,6.4f),Top,(Z-Y*.16f).GetSafeNormal());HouseCampusFallKit::Fan(Solid,TEXT("SD_Dark"),P(0,0,.3f),Bottom,-Z);
 const FVector N=(Z-Y*.16f).GetSafeNormal(),Up=FVector::CrossProduct(N,X).GetSafeNormal(),Dial=P(0,-3.1f,6.03f)+N*.10f;
 Detail.Cylinder(TEXT("SD_PhoneMetal"),Dial,Dial+N*.20f,7.35f,64,FHallUV::World(100));DiscCard(Detail,TEXT("SD_CoachWallDetails"),Dial+N*.23f,X,Up,7.20f,2);
 // A real perforated finger wheel: radial sectors are meshed around ten open circular holes.
 const FVector Wheel=Dial+N*.65f;
 auto W=[&](FVector2D Q,float H=0.f){return Wheel+X*Q.X+Up*Q.Y+N*H;};
 for(int32 Hole=0;Hole<10;++Hole){const float Angle=(45.f+Hole*30.f)*PI/180.f;const FVector2D C(FMath::Cos(Angle)*4.5f,FMath::Sin(Angle)*4.5f);
  auto Boundary=[&](float T){const FVector2D D(FMath::Cos(T),FMath::Sin(T));float Distance=100.f;const float Dot=FVector2D::DotProduct(C,D);Distance=-Dot+FMath::Sqrt(FMath::Max(0.f,Dot*Dot+6.25f*6.25f-float(C.SizeSquared())));const float Inner=Dot*Dot-(float(C.SizeSquared())-2.55f*2.55f);if(Inner>0){const float Hit=-Dot-FMath::Sqrt(Inner);if(Hit>0)Distance=FMath::Min(Distance,Hit);}for(float Side:{-1.f,1.f}){const float A=Angle+Side*PI/12;const FVector2D Ray(FMath::Cos(A),FMath::Sin(A));const float Den=D.X*Ray.Y-D.Y*Ray.X;if(FMath::Abs(Den)>.0001f){const float Hit=-(C.X*Ray.Y-C.Y*Ray.X)/Den;if(Hit>0)Distance=FMath::Min(Distance,Hit);}}return C+D*Distance;};
  for(int32 K=0;K<24;++K){const float A0=K*2*PI/24,A1=(K+1)*2*PI/24;const FVector2D H0=C+FVector2D(FMath::Cos(A0),FMath::Sin(A0))*.92f,H1=C+FVector2D(FMath::Cos(A1),FMath::Sin(A1))*.92f,B0=Boundary(A0),B1=Boundary(A1);Detail.Quad(Body,W(H0),W(H1),W(B1),W(B0),N,FHallUV::World(30));Detail.Quad(TEXT("SD_PhoneMetal"),W(H0),W(H0,-.29f),W(H1,-.29f),W(H1),-(X*FMath::Cos((A0+A1)*.5f)+Up*FMath::Sin((A0+A1)*.5f)),FHallUV::World(30));}
 }
 for(int32 K=0;K<16;++K){const float A=(330.f+K*60.f/16)*PI/180.f,B=(330.f+(K+1)*60.f/16)*PI/180.f;Detail.Quad(Body,W({FMath::Cos(A)*2.55f,FMath::Sin(A)*2.55f}),W({FMath::Cos(A)*6.25f,FMath::Sin(A)*6.25f}),W({FMath::Cos(B)*6.25f,FMath::Sin(B)*6.25f}),W({FMath::Cos(B)*2.55f,FMath::Sin(B)*2.55f}),N,FHallUV::World(30));}
 Detail.Cylinder(TEXT("SD_PhoneMetal"),Dial+N*.27f,Dial+N*.70f,2.55f,48,FHallUV::World(100));DiscCard(Detail,TEXT("SD_CoachWallDetails"),Dial+N*.73f,X,Up,2.35f,2);
 // The central exchange card uses a separate cropped area of the same atlas.
 {auto& S=Detail.Sec(TEXT("SD_CoachWallDetails"));for(int32 J=S.UV.Num()-64*3;J<S.UV.Num();++J){const FVector2D Old=S.UV[J];S.UV[J]={.25f+(Old.X-.25f)*.3264f,.75f+(Old.Y-.75f)*.3264f};}}
 Detail.Cylinder(TEXT("SD_PhoneMetal"),Dial+X*6.5f-Up*2.1f+N*.30f,Dial+X*6.5f-Up*2.1f+N*1.35f,.75f,10,FHallUV::World(100));
 // Curved handset, flared ear/mouth caps, cradle plungers and the moulding seam.
 for(float Side:{-1.f,1.f}){
  Solid.Cylinder(Body,P(Side*7.1f,8.0f,7.5f),P(Side*7.1f,8.0f,9.1f),1.0f,16,FHallUV::World(100));Detail.Cylinder(TEXT("SD_PhoneMetal"),P(Side*7.1f,8.0f,9.05f),P(Side*7.1f,8.0f,9.5f),.65f,12,FHallUV::World(100));
  const FVector Cup=P(Side*8.7f,8.1f,9.0f);const int32 First=Solid.Sec(Body).V.Num(),FirstIndex=Solid.Sec(Body).I.Num();HouseCampusFallKit::Lathe(Solid,Body,FVector::ZeroVector,{{0,0},{3.6f,0},{3.9f,.7f},{3.8f,1.5f},{3.3f,2.2f},{2.45f,4.8f},{2.45f,5.7f},{0,5.7f}},32);auto& S=Solid.Sec(Body);const FQuat Tilt=FQuat::FindBetweenNormals(Z,(Z+X*(Side*.13f)).GetSafeNormal());for(int32 J=First;J<S.V.Num();++J){const FVector Local=S.V[J],Out=-S.N[J],Radial=FVector(Local.X,Local.Y,0).GetSafeNormal();S.V[J]=Cup+Tilt.RotateVector(Local);S.N[J]=Tilt.RotateVector(Radial*FMath::Sqrt(Out.X*Out.X+Out.Y*Out.Y)+Z*Out.Z);S.T[J].TangentX=Tilt.RotateVector(S.T[J].TangentX);}for(int32 J=FirstIndex;J<S.I.Num();J+=3)Swap(S.I[J+1],S.I[J+2]);
  for(int32 K=0;K<32;++K){const float A=K*2*PI/32,B=(K+1)*2*PI/32;Detail.Cylinder(TEXT("SD_Dark"),Cup+X*(FMath::Cos(A)*3.86f)+Y*(FMath::Sin(A)*3.86f)+Z*.75f,Cup+X*(FMath::Cos(B)*3.86f)+Y*(FMath::Sin(B)*3.86f)+Z*.75f,.055f,5,FHallUV::World(100));}
 }
 auto Grip=[&](float T){return P(-8.7f+17.4f*T,8.1f,13.6f+2.2f*FMath::Sin(T*PI));};
 for(int32 J=0;J<24;++J){const float T0=J/24.f,T1=(J+1)/24.f;const FVector Tangent=(Grip(FMath::Min(1.f,T0+.01f))-Grip(FMath::Max(0.f,T0-.01f))).GetSafeNormal(),V=FVector::CrossProduct(Tangent,Y).GetSafeNormal();for(int32 K=0;K<12;++K){const float A=K*2*PI/12,B=(K+1)*2*PI/12;const FVector D0=Y*(FMath::Cos(A)*1.7f)+V*(FMath::Sin(A)*1.6f),D1=Y*(FMath::Cos(B)*1.7f)+V*(FMath::Sin(B)*1.6f);Solid.Quad(Body,Grip(T0)+D0,Grip(T0)+D1,Grip(T1)+D1,Grip(T1)+D0,(D0+D1).GetSafeNormal(),FHallUV::World(30));auto& S=Solid.Sec(Body);const int32 First=S.N.Num()-4;S.N[First]=S.N[First+3]=D0.GetSafeNormal();S.N[First+1]=S.N[First+2]=D1.GetSafeNormal();}}
 for(int32 J=0;J<180;++J){auto Q=[&](int32 K){const float T=K/180.f,A=T*30*PI;return P(13+FMath::Sin(T*PI)*2.2f+FMath::Cos(A)*.65f,7-T*25,4.8f*(1-T)+.85f+FMath::Sin(A)*.65f);};Detail.Cylinder(Body,Q(J),Q(J+1),.19f,6,FHallUV::World(100));}
 Detail.Cylinder(Body,P(10.2f,8.1f,10.2f),P(13.65f,7,5.65f),.25f,8,FHallUV::World(100));
}

void Word(FHallMesh& M,FString Text,FVector C,FVector Right,FVector N,float Height,FName Ink=TEXT("SD_Cream"))
{const float Pitch=Height*.82f;const FVector Left=C-Right*((Text.Len()-1)*Pitch*.5f+Height/3.f);for(int32 I=0;I<Text.Len();++I)for(const auto& Stroke:HallGlyph(Text[I]))for(int32 J=1;J<Stroke.Num();++J){auto P=[&](FVector2D Q){return Left+Right*(I*Pitch+Q.X*Height/6.f)+FVector(0,0,Q.Y*Height/6.f);};HouseCampusFallKit::Bar(M,Ink,P(Stroke[J-1]),P(Stroke[J]),Height*.08f,.15f,N);}}
// Construction details are merged into the STA loader's existing meshes and freed with the room.
void Screw(FHallMesh& M,FVector C,FVector N,float Radius=.45f)
{
 const FVector R(-N.Y,N.X,0);M.Cylinder(TEXT("SD_Aluminium"),C,C+N*.25f,Radius,8,FHallUV::World(100));
 HouseCampusFallKit::Bar(M,TEXT("SD_Dark"),C+N*.28f-R*Radius*.65f,C+N*.28f+R*Radius*.65f,.10f,.08f,N);
}
void Piping(FHallMesh& M,FName Key,FVector Centre,FVector X,FVector Y,float HalfX,float HalfY,float Z,float Radius)
{for(float S:{-1.f,1.f}){M.Cylinder(Key,Centre+X*(S*HalfX)-Y*HalfY+FVector(0,0,Z),Centre+X*(S*HalfX)+Y*HalfY+FVector(0,0,Z),Radius,6,FHallUV::World(100));M.Cylinder(Key,Centre+Y*(S*HalfY)-X*HalfX+FVector(0,0,Z),Centre+Y*(S*HalfY)+X*HalfX+FVector(0,0,Z),Radius,6,FHallUV::World(100));}}
void Jersey(FHallMesh& M,FVector C,FVector Along,FVector N,int32 Number)
{
 const TArray<FVector2D> Shape={{-10,0},{10,0},{12,38},{18,30},{23,43},{12,54},{5,57},{-5,57},{-12,54},{-23,43},{-18,30},{-12,38}};
 // A shaped, shallow garment: shoulder/sleeve outline, side thickness, collar and sewn hem.
 TArray<FVector> Front;for(const auto& Q:Shape)Front.Add(C+Along*(Q.X*.65f)+FVector(0,0,Q.Y)+N*(1.2f+FMath::Cos(Q.X*.2f)*.65f));
 HouseCampusFallKit::Fan(M,TEXT("SD_Jersey"),C+N*2+FVector(0,0,28),Front,N);
 for(int32 J=0;J<Shape.Num();++J){const int32 K=(J+1)%Shape.Num();M.Quad(TEXT("SD_Jersey"),Front[J]-N*1.5f,Front[K]-N*1.5f,Front[K],Front[J],FVector::CrossProduct(Front[K]-Front[J],N).GetSafeNormal(),FHallUV::World(80));}
 HouseCampusFallKit::Bar(M,TEXT("SD_FabricEdge"),C-Along*9+N*2+FVector(0,0,1),C+Along*9+N*2+FVector(0,0,1),.6f,.15f,N);
 const TArray<FVector> Collar={C-Along*5+N*2+FVector(0,0,56),C+N*2+FVector(0,0,51),C+Along*5+N*2+FVector(0,0,56)};
 for(int32 J=1;J<Collar.Num();++J)M.Cylinder(TEXT("SD_Cream"),Collar[J-1],Collar[J],.5f,6,FHallUV::World(100));
 Word(M,FString::FromInt(Number),C+N*2.1f+FVector(0,0,24),Along,N,13.f);
}
}
void HouseStadiumDetail::WoodLockers(FHallMesh& Solid,FHallMesh& Detail,FVector Origin,FVector Along,int32 Count,FVector Facing)
{
 namespace I=HouseStadiumDetailInteriorsImpl;const FRotator R=Along.Rotation();
 const TCHAR* Names[]={TEXT("WARD"),TEXT("BELL"),TEXT("REED"),TEXT("FINCH"),TEXT("MASON"),TEXT("GREER"),TEXT("COLE"),TEXT("ASH"),TEXT("HOLT"),TEXT("DRAKE")};
 for(int32 L=0;L<Count;++L)
 {
  const FVector C=Origin+Along*(L*38+19)+Facing*23;const FName Oak=L%3==0?TEXT("SD_LockerOakLight"):L%3==1?TEXT("SD_LockerOak"):TEXT("SD_LockerOakDark");
  auto Board=[&](FVector P,FVector D){I::SoftBox(Solid,Oak,P,D,R,.35f);};
  Board(C+FVector(0,0,4),{37,46,8});Board(C+FVector(0,0,182),{37,46,8});
  for(float S:{-1.f,1.f}){Board(C+Along*(S*18)+FVector(0,0,94),{2,46,174});Board(C+Along*(S*16.7f)+Facing*23+FVector(0,0,94),{2.7f,2,174});}
  for(int32 P=0;P<3;++P)Board(C-Facing*22+Along*((P-1)*11.7f)+FVector(0,0,94),{11.4f,2,174});
  Board(C+FVector(0,0,152),{34,45,3});Board(C+Facing*23+FVector(0,0,157),{34,2.2f,9});
  Board(C+Facing*21+FVector(0,0,11),{34,3,6});
  for(float S:{-1.f,1.f}){I::Screw(Detail,C+Facing*24.3f+Along*(S*16.7f)+FVector(0,0,18),Facing);I::Screw(Detail,C+Facing*24.3f+Along*(S*16.7f)+FVector(0,0,177),Facing);}
  I::SoftBox(Detail,TEXT("SD_NameBrass"),C+Facing*23.5f+FVector(0,0,171),{30,1,10},R,.3f);
  I::Box(Detail,TEXT("SD_Dark"),C+Facing*24.05f+FVector(0,0,171),{25,.12f,7},R);
  I::Word(Detail,Names[L%10],C+Facing*24.2f+FVector(0,0,169),Along,Facing,3.5f);
  for(float S:{-1.f,1.f})I::Screw(Detail,C+Facing*24.1f+Along*(S*13.6f)+FVector(0,0,171),Facing,.35f);
  if(Step()>=7){FRandomStream Wear(1931+L*71+int32(FMath::Abs(Origin.X)));for(float Side:{-1.f,1.f}){const FVector P=C+Along*(Side*17.7f)+Facing*24.06f+FVector(0,0,Wear.FRandRange(65,128));const float Width=Wear.FRandRange(.20f,.45f),Height=Wear.FRandRange(3,9);Detail.Quad(TEXT("SD_WoodEdge"),P-Along*Width,P+Along*Width+FVector(0,0,1.3f),P+Along*Width*.25f+FVector(0,0,Height),P-Along*Width*.35f+FVector(0,0,Height*.85f),Facing,FHallUV::World(100));}}
  // Open shelf back has real ventilation slots, a hanger rail and a double coat hook.
  Detail.Cylinder(TEXT("SD_Steel"),C-Along*15-Facing*4+FVector(0,0,142),C+Along*15-Facing*4+FVector(0,0,142),.7f,8,FHallUV::World(100));
  for(float S:{-1.f,1.f}){const FVector Hook=C-Facing*18+Along*(S*4)+FVector(0,0,133);Detail.Cylinder(TEXT("SD_Aluminium"),Hook,Hook+Facing*7,.7f,8,FHallUV::World(100));Detail.Cylinder(TEXT("SD_Aluminium"),Hook+Facing*7,Hook+Facing*8+FVector(0,0,3),.7f,8,FHallUV::World(100));}
  for(int32 J=0;J<4;++J)I::Box(Detail,TEXT("SD_Dark"),C-Facing*20.8f+Along*((J-1.5f)*5.5f)+FVector(0,0,167),{.6f,.15f,8},R);
  if(L%3!=1)I::Jersey(Detail,C-Facing*10+FVector(0,0,74+(L%2)*4),Along,Facing,12+(L*7)%77);
  if(L%5==1){I::SoftBox(Detail,TEXT("SD_Towel"),C-Facing*2+FVector(0,0,157),{23,29,6},R,.8f);I::Piping(Detail,TEXT("SD_FabricEdge"),C-Facing*2,Along,Facing,10,12,160,.2f);}
  if(L%7==3){Detail.Cylinder(TEXT("SD_Cream"),C+FVector(0,0,9),C+FVector(0,0,27),3.3f,12,FHallUV::World(100));I::Box(Detail,TEXT("SD_Gold"),C+Facing*3.3f+FVector(0,0,18),{5,.2f,8},R);}
 }
}
void HouseStadiumDetail::LockerBench(FHallMesh& Solid,FHallMesh& Detail,FVector Start,FVector Along,float Span,FVector Facing)
{
 namespace I=HouseStadiumDetailInteriorsImpl;const FVector C=Start+Along*(Span*.5f);const FRotator R=Along.Rotation();
 for(int32 J=0;J<3;++J)I::SoftBox(Solid,Step()>=7?TEXT("SD_BenchWood"):J==1?TEXT("SD_LockerOakLight"):TEXT("SD_LockerOak"),C+Facing*((J-1)*11.8f)+FVector(0,0,44),{Span,11.2f,4},R,.5f);
 for(float U=20;U<Span;U+=120){const FVector P=Start+Along*U;for(float S:{-1.f,1.f}){I::Box(Solid,TEXT("SD_Steel"),P+Facing*(S*12)+FVector(0,0,20),{4,4,40},R);I::Box(Solid,TEXT("SD_Steel"),P+Facing*(S*12)+FVector(0,0,1),{15,9,2},R);I::Screw(Detail,P+Facing*(S*13.9f)+FVector(0,0,38),Facing*S,.6f);}I::Box(Solid,TEXT("SD_Steel"),P+FVector(0,0,40),{7,35,3},R);Detail.Cylinder(TEXT("SD_Steel"),P-Along*20+FVector(0,0,20),P+FVector(0,0,40),1.5f,8,FHallUV::World(100));}
 I::Box(Solid,TEXT("SD_Steel"),C+FVector(0,0,18),{Span-30,3,3},R);
}
void HouseStadiumDetail::LockerHelmet(FHallMesh& Solid,FHallMesh& Detail,FVector Base,FVector Facing)
{
 namespace I=HouseStadiumDetailInteriorsImpl;const FVector X=Facing.GetSafeNormal2D(),Y(-X.Y,X.X,0);
 auto P=[&](float Xv,float Yv,float Z){return Base+X*Xv+Y*Yv+FVector(0,0,Z);};
 auto Shell=[&](float T,float A){return P(14*FMath::Sin(T)*FMath::Cos(A),11.7f*FMath::Sin(T)*FMath::Sin(A),16+15*FMath::Cos(T));};
 auto Limit=[](float A){return 2.24f-1.04f*FMath::Pow(FMath::Max(0.f,FMath::Cos(A)),3.f);};
 for(int32 J=0;J<32;++J){const float A=J*2*PI/32,B=(J+1)*2*PI/32;for(int32 K=0;K<12;++K){const float T0=(K+.01f)/12,T1=(K+1.f)/12;const FVector V[]={Shell(T0*Limit(A),A),Shell(T0*Limit(B),B),Shell(T1*Limit(B),B),Shell(T1*Limit(A),A)};const FVector N=((V[0]+V[1]+V[2]+V[3])*.25f-P(0,0,16)).GetSafeNormal();Solid.Quad(TEXT("SD_HelmetShell"),V[0],V[1],V[2],V[3],N,FHallUV::World(100));{auto& S=Solid.Sec(TEXT("SD_HelmetShell"));const int32 First=S.N.Num()-4;for(int32 I=0;I<4;++I){const FVector D=V[I]-P(0,0,16);S.N[First+I]=(X*(FVector::DotProduct(D,X)/(14*14))+Y*(FVector::DotProduct(D,Y)/(11.7f*11.7f))+FVector::UpVector*(D.Z/(15*15))).GetSafeNormal();}}}
  const FVector A0=Shell(Limit(A),A),B0=Shell(Limit(B),B);Detail.Cylinder(TEXT("SD_PaddingDark"),A0,B0,.65f,6,FHallUV::World(100));}
 // A narrow gold stripe follows the shell over the crown, rather than a floating cuboid.
 for(int32 J=0;J<28;++J){const float A=-2.18f+J*3.36f/28,B=A+3.36f/28;auto Stripe=[&](float T,float Side){return P(14.12f*FMath::Sin(T),Side,16+15.12f*FMath::Cos(T));};Detail.Quad(TEXT("SD_Gold"),Stripe(A,-.8f),Stripe(A,.8f),Stripe(B,.8f),Stripe(B,-.8f),(X*FMath::Sin((A+B)*.5f)+FVector::UpVector*FMath::Cos((A+B)*.5f)),FHallUV::World(100));}
 for(float Side:{-1.f,1.f}){
  const FVector SideN=Y*Side;Detail.Cylinder(TEXT("SD_Dark"),P(0,Side*11.3f,16),P(0,Side*11.8f,16),2.1f,16,FHallUV::World(100));I::Screw(Detail,P(8,Side*10.4f,18),SideN,.7f);
  for(float Z:{8.f,13.f})Detail.Cylinder(TEXT("SD_Aluminium"),P(6,Side*10,Z+3),P(18,Side*10,Z),.58f,8,FHallUV::World(100));
  Detail.Cylinder(TEXT("SD_Aluminium"),P(17,Side*10,8),P(18.2f,Side*10,17),.58f,8,FHallUV::World(100));
  Detail.Cylinder(TEXT("SD_Cream"),P(-1,Side*10,11),P(14,Side*8,1.4f),.65f,8,FHallUV::World(100));
  for(float Z:{23.f,26.f})I::Box(Detail,TEXT("SD_Dark"),P(-3,Side*9.3f,Z),{3.5f,.2f,.65f},X.Rotation());
 }
 for(float Z:{8.f,13.f})for(int32 J=0;J<12;++J){const float A=-HALF_PI+J*PI/12,B=A+PI/12;Detail.Cylinder(TEXT("SD_Aluminium"),P(17+4*FMath::Cos(A),10*FMath::Sin(A),Z),P(17+4*FMath::Cos(B),10*FMath::Sin(B),Z),.58f,8,FHallUV::World(100));}
 I::SoftBox(Detail,TEXT("SD_Cream"),P(14,0,2),{4,12,3},X.Rotation(),.5f);
}
// Period utility fixtures and one authored coach's corner; all additions belong to STA's loader.
void HouseStadiumDetail::RoomFixture(FHallMesh& Glow,FHallMesh& Detail,FVector Centre,FVector Along)
{
 namespace I=HouseStadiumDetailInteriorsImpl;const FVector Side(-Along.Y,Along.X,0);const FRotator R=Along.Rotation();Centre.Z=170.f;
 I::SoftBox(Detail,TEXT("SD_RoomBeam"),Centre,{125,20,5},R,.6f);
 for(float S:{-1.f,1.f}){
  const FVector A=Centre-Along*54+Side*(S*5)+FVector(0,0,-3.7f),B=Centre+Along*54+Side*(S*5)+FVector(0,0,-3.7f);
  Glow.Cylinder(S>0&&int32(FMath::Abs(Centre.X))%3==0?TEXT("SD_RoomTubeOld"):TEXT("SD_RoomTube"),A,B,1.5f,12,FHallUV::World(100));
  for(const FVector P:{A,B})I::SoftBox(Detail,TEXT("SD_Cream"),P,{5,5,5},R,.6f);
 }
 for(float S:{-1.f,1.f})I::Screw(Detail,Centre+Along*(S*47)+FVector(0,0,-2.6f),-FVector::UpVector,.6f);
}
void HouseStadiumDetail::RoomIdentity(FHallMesh& Solid,FHallMesh& Detail)
{
 if(!Enabled()||Step()<7)return;namespace I=HouseStadiumDetailInteriorsImpl;
 // Shallow structural ribs fit above the 220cm clear standing envelope.
 for(float Angle:{51.f,54.5f,62.f,65.f}){
  const FVector A=I::At(Angle,.692f,170.f),B=I::At(Angle,.888f,170.f),Along=(B-A).GetSafeNormal();const float Length=FVector::Dist(A,B);const FVector C=(A+B)*.5f;
  I::Box(Solid,TEXT("SD_RoomBeam"),C,{Length,12,8},Along.Rotation());I::Box(Solid,TEXT("SD_RoomBeam"),C+FVector(0,0,-4.5f),{Length,19,2},Along.Rotation());
 }
 // Supply/return pipes follow the actual clipped inner-wall chords, never across the tunnel opening below headroom.
 for(int32 K=5;K<=7;++K){const float T0=FMath::Max(0.f,(50.f-K*9.f)/9.f),T1=FMath::Min(1.f,(66.f-K*9.f)/9.f);if(T1<=T0)continue;
  const FVector L=I::At(K*9.f,.695f,0),R=I::At((K+1)*9.f,.695f,0),Along=(R-L).GetSafeNormal(),In(Along.Y,-Along.X,0);const FVector A=FMath::Lerp(L,R,T0)+In*7,B=FMath::Lerp(L,R,T1)+In*7;
  for(float Z:{154.f,163.f}){auto Run=[&](FVector Start,FVector End){Solid.Cylinder(TEXT("SD_RoomPipe"),Start+FVector(0,0,Z),End+FVector(0,0,Z),1.8f,10,FHallUV::World(100));for(float U=40;U<FVector::Dist(Start,End);U+=160){const FVector P=Start+Along*U+FVector(0,0,Z);Detail.Cylinder(TEXT("SD_Steel"),P-Along*1.5f,P+Along*1.5f,2.4f,10,FHallUV::World(100));}};
   if(K==6){const FVector C=(L+R)*.5f+In*7,Left=C-Along*177,Right=C+Along*177;Run(A,Left);Run(Right,B);for(const FVector P:{Left,Right}){Solid.Cylinder(TEXT("SD_RoomPipe"),P+FVector(0,0,Z),P-In*12+FVector(0,0,Z),1.8f,10,FHallUV::World(100));Detail.Cylinder(TEXT("SD_RoomBeam"),P-In*5+FVector(0,0,Z),P-In*6+FVector(0,0,Z),4.f,12,FHallUV::World(100));}}else Run(A,B);}
 }
 const FVector Along=(I::At(50,.86f,0)-I::At(50,.84f,0)).GetSafeNormal(),Into(-Along.Y,Along.X,0);
 // Split the faded stripe around the coach-office opening; no trim or paper spans the doorway.
 for(FVector2D Range:{FVector2D(-295,-62),FVector2D(62,600)}){const FVector Stripe=CoachPoint((Range.X+Range.Y)*.5f,22.2f,133);HouseCampusFallKit::Card(Detail,TEXT("SD_RoomStripe"),Stripe,Along,FVector::UpVector,Range.Y-Range.X,13,Into);}
 const FVector Pipe=I::At(50.f,.715f,0)+Into*23;Solid.Cylinder(TEXT("SD_RoomPipe"),Pipe+FVector(0,0,-38),Pipe+FVector(0,0,163),2.3f,12,FHallUV::World(100));
 const FVector Valve=Pipe+Into*4+FVector(0,0,34);Detail.Cylinder(TEXT("SD_Steel"),Valve-Into*5,Valve,1.1f,10,FHallUV::World(100));for(int32 J=0;J<20;++J){const float A=J*2*PI/20,B=(J+1)*2*PI/20;Detail.Cylinder(TEXT("SD_Rust"),Valve+Along*(FMath::Cos(A)*5)+FVector::UpVector*(FMath::Sin(A)*5),Valve+Along*(FMath::Cos(B)*5)+FVector::UpVector*(FMath::Sin(B)*5),.65f,6,FHallUV::World(100));}
 // Two off-white towels settle into wrinkles at their original dressing positions, with no new collision footprint.
 for(float Angle:{54.f,61.5f}){const FVector C=I::At(Angle,.82f,-54.8f);auto P=[&](int32 X,int32 Y){const float U=X/10.f,V=Y/7.f,Envelope=FMath::Sin(U*PI)*FMath::Sin(V*PI);return C+FVector((U-.5f)*50,(V-.5f)*34,.12f+Envelope*(1.7f+1.2f*FMath::Sin(U*21+V*17)));};for(int32 X=0;X<10;++X)for(int32 Y=0;Y<7;++Y)Detail.Quad(TEXT("SD_Towel"),P(X,Y),P(X+1,Y),P(X+1,Y+1),P(X,Y+1),FVector::UpVector,FHallUV::World(60));}
}

void HouseStadiumDetail::Interiors(FHallMesh& Solid,FHallMesh& Clutter,FHallMesh& Glow)
{
 if(!Enabled()||Step()<6)return;namespace I=HouseStadiumDetailInteriorsImpl;
 constexpr float Floor=-55.f;
 // Trainer's table by the east end wall, outside the existing64.3/58.5degree route.
 const FVector Table=I::At(51.8f,.823f,Floor);const FRotator Rot(0,141.8f,0);const FVector X=Rot.Vector(),Y(-X.Y,X.X,0);
 I::SoftBox(Solid,TEXT("SD_LockerOak"),Table+FVector(0,0,68),{183,64,10},Rot,.8f);I::SoftBox(Solid,TEXT("SD_Padding"),Table+FVector(0,0,75),{185,66,6},Rot,1.2f);
 for(float A:{-76.f,76.f})for(float B:{-23.f,23.f})I::Box(Solid,TEXT("SD_LockerOak"),Table+X*A+Y*B+FVector(0,0,32.5f),{7,7,65},Rot);
 for(int32 J=0;J<3;++J){const FVector P=Table+X*(J*17-35)+Y*(J==1?9.f:-7.f)+FVector(0,0,78);const TArray<FVector2D> Roll={{3,0},{3,6},{7,6},{7,0},{3,0}};HouseCampusFallKit::Lathe(Clutter,TEXT("SD_Cream"),P,Roll,24);}
 // Trainer's joinery, lower shelf, corner plates and upholstery piping; the footprint and78cm top stay fixed.
 for(float Side:{-1.f,1.f}){
  I::SoftBox(Solid,TEXT("SD_LockerOak"),Table+Y*(Side*25)+FVector(0,0,57),{159,4,13},Rot,.4f);
  I::SoftBox(Solid,TEXT("SD_LockerOakLight"),Table+X*(Side*76)+FVector(0,0,24),{6,49,5},Rot,.4f);
  for(float End:{-1.f,1.f})I::Screw(Clutter,Table+X*(End*76)+Y*(Side*27.2f)+FVector(0,0,58),Y*Side,.65f);
 }
 for(int32 J=0;J<5;++J)I::SoftBox(Solid,TEXT("SD_LockerOak"),Table+Y*((J-2)*9.5f)+FVector(0,0,24),{152,9,2},Rot,.25f);
 I::Piping(Clutter,TEXT("SD_FabricEdge"),Table,X,Y,90.6f,31.5f,73.5f,.38f);
 I::Piping(Clutter,TEXT("SD_FabricEdge"),Table,X,Y,90.8f,32.4f,76.8f,.30f);
 for(float End:{-1.f,1.f})for(int32 J=0;J<38;++J){const FVector A=Table+X*(-88+J*4.65f)+Y*(End*32.7f)+FVector(0,0,74.3f);HouseCampusFallKit::Bar(Clutter,TEXT("SD_PaddingDark"),A,A+X*1.7f,.12f,.10f,Y*End);}
 // Tape tails, scissors and a labelled amber bottle break the previous identical row of props.
 const FVector Tape=Table-X*35+FVector(0,0,78.1f);HouseCampusFallKit::Card(Clutter,TEXT("SD_Cream"),Tape+Y*9,X,Y,5,18,FVector::UpVector);
 const FVector Bottle=Table+X*47+Y*16+FVector(0,0,78);Clutter.Cylinder(TEXT("SD_BottleAmber"),Bottle,Bottle+FVector(0,0,16),3.4f,16,FHallUV::World(100));Clutter.Cylinder(TEXT("SD_Cream"),Bottle+FVector(0,0,16),Bottle+FVector(0,0,19),2.4f,16,FHallUV::World(100));HouseCampusFallKit::Card(Clutter,TEXT("SD_TrainerLabel"),Bottle-Y*3.45f+FVector(0,0,9),X,FVector::UpVector,5.7f,7,-Y);
 I::SoftBox(Clutter,TEXT("SD_Towel"),Table+X*8+Y*8+FVector(0,0,29),{43,30,7},Rot,.8f);
 for(float S:{-1.f,1.f})Clutter.Cylinder(TEXT("SD_Aluminium"),Table+X*10+Y*(S*2)+FVector(0,0,78.2f),Table+X*26+Y*(-S*2)+FVector(0,0,78.2f),.35f,8,FHallUV::World(100));
 for(float S:{-1.f,1.f})for(int32 J=0;J<16;++J){const float A=J*2*PI/16,B=(J+1)*2*PI/16;const FVector C=Table+X*8+Y*(S*3.6f)+FVector(0,0,78.2f);Clutter.Cylinder(TEXT("SD_Aluminium"),C+X*(FMath::Cos(A)*2.8f)+Y*(FMath::Sin(A)*1.7f),C+X*(FMath::Cos(B)*2.8f)+Y*(FMath::Sin(B)*1.7f),.32f,8,FHallUV::World(100));}


 // A compact oval steel whirlpool: real hollow walls, rolled rim and a small period agitator.
 // The64.3degree door-to-room line stays clear; leave room to walk behind the tub/locker bench too.
 const FVector Tub=I::At(55.6f,.83f,Floor);const FRotator TR(0,145.6f,0);
 const FVector TX=TR.Vector(),TY(-TX.Y,TX.X,0);
 auto Oval=[&](float A,float RX,float RY,float Z){return Tub+TX*(FMath::Cos(A)*RX)+TY*(FMath::Sin(A)*RY)+FVector(0,0,Z);};
 const auto MetalUV=FHallUV::World(100);
 for(int32 J=0;J<48;++J){const float A=J*2*PI/48,B=(J+1)*2*PI/48;const FVector Out=(TX*(FMath::Cos((A+B)*.5f)/62.f)+TY*(FMath::Sin((A+B)*.5f)/36.f)).GetSafeNormal();
  Solid.Quad(TEXT("SD_TubSteel"),Oval(A,62,36,5),Oval(B,62,36,5),Oval(B,62,36,76),Oval(A,62,36,76),Out,MetalUV);
  {auto& Section=Solid.Sec(TEXT("SD_TubSteel"));const int32 First=Section.N.Num()-4;for(int32 V=0;V<4;++V){const float Angle=(V==0||V==3)?A:B;Section.N[First+V]=(TX*(FMath::Cos(Angle)/62.f)+TY*(FMath::Sin(Angle)/36.f)).GetSafeNormal();}}
  Solid.Quad(TEXT("SD_TubSteel"),Oval(A,58,32,8),Oval(A,58,32,76),Oval(B,58,32,76),Oval(B,58,32,8),-Out,MetalUV);
  Solid.Quad(TEXT("SD_TubSteel"),Oval(A,62,36,76),Oval(B,62,36,76),Oval(B,58,32,76),Oval(A,58,32,76),FVector::UpVector,MetalUV);
  HouseCampusFallKit::Tri(Solid,TEXT("SD_Steel"),Tub+FVector(0,0,8),Oval(A,58,32,8),Oval(B,58,32,8),FVector::UpVector,MetalUV);
  HouseCampusFallKit::Tri(Clutter,TEXT("SD_TubWater"),Tub+FVector(0,0,26),Oval(A,58,32,26),Oval(B,58,32,26),FVector::UpVector,MetalUV);
  Clutter.Cylinder(TEXT("SD_TubSteel"),Oval(A,60,34,78),Oval(B,60,34,78),2.2f,6,MetalUV);
 }
 for(float A:{-44.f,44.f})for(float B:{-21.f,21.f})I::SoftBox(Solid,TEXT("SD_Steel"),Tub+TX*A+TY*B+FVector(0,0,3),{8,8,6},TR,.5f);
 const FVector Motor=Tub-TX*43;I::SoftBox(Solid,TEXT("SD_Cream"),Motor+FVector(0,0,102),{24,27,34},TR,1.5f);Clutter.Cylinder(TEXT("SD_Steel"),Motor+FVector(0,0,28),Motor+FVector(0,0,89),3.f,8,MetalUV);
 I::Box(Clutter,TEXT("SD_Dark"),Motor+TY*14+FVector(0,0,107),{9,1,7},TR);
 const FVector Spill=I::At(55.3f,.809f,Floor+.10f);TArray<FVector> WetEdge;FRandomStream WetSeed(861018);for(int32 J=0;J<20;++J){const float A=J*2*PI/20,Scale=WetSeed.FRandRange(.78f,1.18f);WetEdge.Add(Spill+TX*(FMath::Cos(A)*49*Scale)+TY*(FMath::Sin(A)*33*Scale));}HouseCampusFallKit::Fan(Clutter,TEXT("SD_Wet"),Spill,WetEdge,FVector::UpVector);
 // Real handles, clamp-on agitator mount, dial, ventilation and a safely coiled cable.
 for(float S:{-1.f,1.f}){
  const FVector H=Tub+TY*(S*36);const FVector A=H-TX*15+FVector(0,0,62),B=H+TX*15+FVector(0,0,62);
  for(const FVector P:{A,B})Clutter.Cylinder(TEXT("SD_TubSteel"),P,P+TY*(S*5)+FVector(0,0,4),1.1f,10,MetalUV);
  Clutter.Cylinder(TEXT("SD_TubSteel"),A+TY*(S*5)+FVector(0,0,4),B+TY*(S*5)+FVector(0,0,4),1.1f,10,MetalUV);
  for(float U:{-28.f,28.f})I::Screw(Clutter,Tub+TX*U+TY*(S*29)+FVector(0,0,70),TY*S,.6f);
 }
 for(float S:{-1.f,1.f})I::SoftBox(Solid,TEXT("SD_Steel"),Motor+TY*(S*12)+FVector(0,0,81),{12,5,18},TR,.5f);
 I::Box(Solid,TEXT("SD_TubSteel"),Motor+FVector(0,0,89),{29,34,4},TR);
 const FVector Dial=Motor+TY*14.1f+FVector(0,0,108);Clutter.Cylinder(TEXT("SD_Dark"),Dial,Dial+TY*1.5f,3.4f,24,MetalUV);Clutter.Cylinder(TEXT("SD_Cream"),Dial+TY*1.6f,Dial+TY*2.4f,2.4f,24,MetalUV);
 HouseCampusFallKit::Bar(Clutter,TEXT("SD_Dark"),Dial+TY*2.5f,Dial+TY*2.5f+TX*1.6f+FVector(0,0,1.1f),.3f,.12f,TY);
 for(int32 J=0;J<7;++J){const float A=-PI*.7f+J*PI*1.4f/6;Clutter.Cylinder(TEXT("SD_Dark"),Dial+TY*1.6f+(TX*FMath::Sin(A)+FVector::UpVector*FMath::Cos(A))*4.1f,Dial+TY*1.6f+(TX*FMath::Sin(A)+FVector::UpVector*FMath::Cos(A))*4.7f,.14f,6,MetalUV);}
 for(int32 J=0;J<7;++J)I::Box(Clutter,TEXT("SD_Dark"),Motor+TY*13.65f+TX*((J-3)*2.6f)+FVector(0,0,94),{1.3f,.15f,5},TR);
 HouseCampusFallKit::Card(Clutter,TEXT("SD_AgitatorLabel"),Motor+TY*13.8f+FVector(0,0,119),TX,FVector::UpVector,18,6,TY);
 I::Box(Clutter,TEXT("SD_Dark"),Motor+TY*14.2f+TX*7+FVector(0,0,108),{2.4f,1.6f,6},TR);
 for(float S:{-1.f,1.f})for(float Z:{89.f,116.f})I::Screw(Clutter,Motor+TY*13.6f+TX*(S*9)+FVector(0,0,Z),TY,.45f);
 const FVector Cord=Motor-TX*9-TY*6+FVector(0,0,90);Clutter.Cylinder(TEXT("SD_Dark"),Cord,Cord-TX*7+FVector(0,0,-25),.55f,8,MetalUV);
 for(int32 J=0;J<72;++J){auto P=[&](float T){return Cord-TX*7+FVector(0,0,-25)+TY*(5*FMath::Cos(T))+TX*(5*FMath::Sin(T))+FVector(0,0,-T*.40f);};const float A=J*6*PI/72;Clutter.Cylinder(TEXT("SD_Dark"),P(A),P(A+6*PI/72),.55f,8,MetalUV);}
 // The inspection side also has stamped maintenance markings, vents and a removable rear cover.
 HouseCampusFallKit::Card(Clutter,TEXT("SD_AgitatorLabel"),Motor+TX*12.15f+FVector(0,0,108),-TY,FVector::UpVector,18,7,TX);
 for(float U:{-9.f,9.f})for(float Z:{90.f,115.f})I::Screw(Clutter,Motor+TX*12.05f+TY*U+FVector(0,0,Z),TX,.4f);
 for(int32 J=0;J<6;++J)I::Box(Clutter,TEXT("SD_Dark"),Motor+TX*12.1f+TY*((J-2.5f)*2.7f)+FVector(0,0,97),{.15f,1.4f,4},TR);
 // Drain and graduated inner fill marks are visible through the shallow water.
 Clutter.Cylinder(TEXT("SD_Dark"),Tub+TX*25+FVector(0,0,8.1f),Tub+TX*25+FVector(0,0,8.3f),3,16,MetalUV);
 for(float Z:{30.f,40.f,50.f,60.f})I::Box(Clutter,TEXT("SD_Dark"),Tub+TY*31.8f+FVector(0,0,Z),{6,.15f,.4f},TR);

 // A second small play diagram on the existing locker-room end wall, no room or doorway added.
 const FVector Across=(I::At(50,.86f,0)-I::At(50,.84f,0)).GetSafeNormal(),Out(-Across.Y,Across.X,0),Board=I::At(50.f,.855f,70)+Out*25;
 I::Box(Solid,TEXT("SD_Wood"),Board,{145,8,87},Across.Rotation());HouseCampusFallKit::Card(Clutter,TEXT("SD_Play"),Board+Out*4.2f,Across,FVector::UpVector,135,78,Out);
 // New pads line existing tunnel cheeks only. Opening stays300cm wide;4cm pads leave292cm.
 const FVector MouthA=I::At(54,.695f,Floor),MouthB=I::At(63,.695f,Floor),AcrossTunnel=(MouthB-MouthA).GetSafeNormal(),Rad(AcrossTunnel.Y,-AcrossTunnel.X,0),Mouth=(MouthA+MouthB)*.5f;
 for(float Side:{-1.f,1.f}){I::SoftBox(Solid,TEXT("SD_Padding"),Mouth+AcrossTunnel*(Side*148)+Rad*15+FVector(0,0,105),{90,4,205},Rad.Rotation(),.8f);for(float Z:{35.f,105.f,175.f})Clutter.Cylinder(TEXT("SD_Steel"),Mouth+AcrossTunnel*(Side*145.8f)+Rad*12+FVector(0,0,Z),Mouth+AcrossTunnel*(Side*145.4f)+Rad*12+FVector(0,0,Z),1.2f,6,FHallUV::World(100));}
 for(float Side:{-1.f,1.f}){
  const FVector Face=Mouth+AcrossTunnel*(Side*145.8f)+Rad*15;
  for(float Z:{Floor+38.f,Floor+106.f,Floor+174.f})Clutter.Cylinder(TEXT("SD_PaddingDark"),Face-Rad*43+FVector(0,0,Z),Face+Rad*43+FVector(0,0,Z),.14f,6,FHallUV::World(100));
  for(float Edge:{-43.f,43.f})Clutter.Cylinder(TEXT("SD_FabricEdge"),Face+Rad*Edge+FVector(0,0,Floor+5),Face+Rad*Edge+FVector(0,0,Floor+205),.28f,6,FHallUV::World(100));
 }
 const FVector Torn=I::At(58.5f,.715f,Floor+.12f);
 // A real irregular torn edge, mapped to the authored paper artwork; no black rectangle faking a hole.
 const TArray<FVector2D> Edge={{-95,-23.5f},{90,-23.5f},{85,-15},{95,-6},{83,0},{93,6},{87,14},{95,23.5f},{-95,23.5f}};
 FHallSection& Paper=Clutter.Sec(TEXT("SD_Beat"));for(int32 J=0;J<Edge.Num();++J){const int32 Base=Paper.V.Num();for(FVector2D Q:{FVector2D::ZeroVector,Edge[J],Edge[(J+1)%Edge.Num()]}){Paper.V.Add(Torn+AcrossTunnel*Q.X+Rad*Q.Y);Paper.N.Add(FVector::UpVector);Paper.UV.Add({Q.X/190.f+.5f,.5f-Q.Y/47.f});Paper.T.Add(FProcMeshTangent(AcrossTunnel,false));}if(FVector::CrossProduct(Paper.V[Base+1]-Paper.V[Base],Paper.V[Base+2]-Paper.V[Base]).Z<0)Paper.I.Append({Base,Base+1,Base+2});else Paper.I.Append({Base,Base+2,Base+1});}
 RoomIdentity(Solid,Clutter);
 CoachOffice(Solid,Clutter,Glow);
 UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_INTERIOR sameDoors=1 sameRoute=1 trainer=1 whirlpool=1 tunnelPads=2 coachOffice=1 ownerAuthorizedRoom=1 newLights=0"));
}

// Owner-requested separate office beside the locker room. All coordinates use the existing clipped stadium chords.
namespace HouseStadiumDetailCoachImpl
{
FVector Point(float Degrees,float Radius,float Z)
{
 const float T=(Degrees-45.f)/9.f,A=PI*.25f,B=PI*.30f;
 return {2400+FMath::Lerp(FMath::Cos(A),FMath::Cos(B),T)*5600*Radius,-13300+FMath::Lerp(FMath::Sin(A),FMath::Sin(B),T)*3800*Radius,Z};
}
FVector X(){return (Point(50,.80f,0)-Point(50,.78f,0)).GetSafeNormal();}
FVector Y(){const FVector U=X();return {-U.Y,U.X,0};}
void Paper(FHallMesh& Mesh,FName Key,FVector Centre,FVector Across,FVector Up,FVector N,float Width,float Height,int32 Cell,float Degrees,float Curl=0,int32 Grid=3)
{
 const float Angle=FMath::DegreesToRadians(Degrees);const FVector Right=Across*FMath::Cos(Angle)+Up*FMath::Sin(Angle),Rise=Up*FMath::Cos(Angle)-Across*FMath::Sin(Angle);
 const float Scale=Cell<0?1.f:1.f/Grid,U0=Cell<0?0.f:(Cell%Grid)*Scale,V0=Cell<0?0.f:(Cell/Grid)*Scale,Inset=.001f;
 Mesh.QuadUV(Key,Centre-Right*(Width*.5f)-Rise*(Height*.5f),Centre+Right*(Width*.5f)-Rise*(Height*.5f),Centre+Right*(Width*.5f)+Rise*(Height*.5f)+N*Curl,Centre-Right*(Width*.5f)+Rise*(Height*.5f),N,{U0+Inset,V0+Scale-Inset},{U0+Scale-Inset,V0+Scale-Inset},{U0+Scale-Inset,V0+Inset},{U0+Inset,V0+Inset},Right);
}
void Wall(FHallMesh& Solid,FVector A,FVector B)
{
 namespace I=HouseStadiumDetailInteriorsImpl;const FVector U=(B-A).GetSafeNormal();const float Span=FVector::Dist(A,B)+12;const FVector C=(A+B)*.5f;
 I::Box(Solid,TEXT("SD_RoomPlaster"),C+FVector(0,0,60),{Span,12,230},U.Rotation());
}
}
FVector HouseStadiumDetail::CoachPoint(float U,float V,float Z)
{namespace C=HouseStadiumDetailCoachImpl;return C::Point(50,.754f,Z)+C::X()*U+C::Y()*V;}
bool HouseStadiumDetail::CoachInside(FVector P)
{
 if(!Enabled()||Step()<7||P.Z< -65||P.Z>175)return false;namespace C=HouseStadiumDetailCoachImpl;
 const FVector Poly[]={C::Point(45,.720f,0),C::Point(45,.815f,0),C::Point(50,.815f,0),C::Point(50,.720f,0)};
 bool Positive=false,Negative=false;for(int32 J=0;J<4;++J){const FVector A=Poly[J],B=Poly[(J+1)%4];const double Cross=(B.X-A.X)*(P.Y-A.Y)-(B.Y-A.Y)*(P.X-A.X);Positive|=Cross>.01;Negative|=Cross<-.01;if(Positive&&Negative)return false;}return true;
}
void HouseStadiumDetail::CoachPartition(FHallMesh& Solid,FVector Start,FVector End,FVector Along,FVector Side,float Shift)
{
 namespace I=HouseStadiumDetailInteriorsImpl;const FVector Normal=Side*(Shift>0?1.f:-1.f);const float Span=FVector::Dist(Start,End),Door=FVector::DotProduct(CoachPoint(0,0,0)-Start,Along);
 auto Piece=[&](float L,float R,float Bottom,float Top,FName Key,float Depth,float Offset){if(R<=L)return;const FVector P=Start+Along*((L+R)*.5f)+Side*Shift+Normal*Offset+FVector(0,0,(Bottom+Top)*.5f);I::Box(Solid,Key,P,{R-L,Depth,Top-Bottom},Along.Rotation());};
 for(FVector2D Range:{FVector2D(0,Door-60),FVector2D(Door+60,Span)}){
  Piece(Range.X,Range.Y,-60,177,TEXT("SD_RoomPlaster"),20,0);
  Piece(Range.X,Range.Y,-55,43,TEXT("SD_LockerOakDark"),2.4f,11.1f);
  for(float U=Range.X+6;U<Range.Y;U+=12)Piece(U-5.7f,FMath::Min(U+5.7f,float(Range.Y)),-53,41,TEXT("SD_LockerOak"),.8f,12.5f);
  for(float Z:{-50.f,44.f})Piece(Range.X,Range.Y,Z-2.5f,Z+2.5f,TEXT("SD_LockerOakLight"),3.6f,11.9f);
 }
 Piece(Door-60,Door+60,158,177,TEXT("SD_RoomPlaster"),20,0);
 for(float S:{-1.f,1.f})I::SoftBox(Solid,TEXT("SD_LockerOakDark"),CoachPoint(S*58.5f,10,51.5f),{3,26,213},Along.Rotation(),.35f);
 I::SoftBox(Solid,TEXT("SD_LockerOakDark"),CoachPoint(0,10,156.5f),{120,26,3},Along.Rotation(),.35f);
}
void HouseStadiumDetail::CoachOffice(FHallMesh& Solid,FHallMesh& Detail,FHallMesh& Glow)
{
 if(!Enabled()||Step()<7)return;namespace I=HouseStadiumDetailInteriorsImpl;namespace C=HouseStadiumDetailCoachImpl;
 const FVector X=C::X(),Y=C::Y();const FRotator R=X.Rotation();const FVector A=C::Point(45,.720f,-55),B=C::Point(45,.815f,-55),D=C::Point(50,.720f,-55),E=C::Point(50,.815f,-55);
 Solid.Quad(TEXT("SD_RoomFloor"),A,B,E,D,FVector::UpVector,FHallUV::World(250));
 Solid.Quad(TEXT("SD_RoomCeiling"),A+FVector(0,0,230),D+FVector(0,0,230),E+FVector(0,0,230),B+FVector(0,0,230),-FVector::UpVector,FHallUV::World(240));
 for(const TPair<FVector,FVector>& Pair:{TPair<FVector,FVector>(A,B),TPair<FVector,FVector>(A,D),TPair<FVector,FVector>(B,E)}){
  C::Wall(Solid,FVector(Pair.Key.X,Pair.Key.Y,0),FVector(Pair.Value.X,Pair.Value.Y,0));
  const FVector U=(Pair.Value-Pair.Key).GetSafeNormal(),N(-U.Y,U.X,0),Mid=(Pair.Key+Pair.Value)*.5f;const float L=FVector::Dist(Pair.Key,Pair.Value)+8;
  I::SoftBox(Solid,TEXT("SD_FurnitureOakDark"),Mid+FVector(0,0,5),{L,16,10},U.Rotation(),.4f);
  for(float Z:{100.f,226.f})I::SoftBox(Detail,TEXT("SD_FurnitureOakDark"),Mid+FVector(0,0,Z),{L,14,3},U.Rotation(),.3f);
  (void)N;
 }
 RoomFixture(Glow,Detail,CoachPoint(100,-140,170),X);
 // A broad 1930s oak pedestal desk: knee space, separate drawer fronts, brass pulls, recessed panels and worn top edges.
 const FRotator DR=R+FRotator(0,3,0);const FVector DX=DR.Vector(),DY(-DX.Y,DX.X,0);
 const FVector Desk=CoachPoint(153,-168,-55);auto P=[&](float U,float V,float Z){return Desk+DX*U+DY*V+FVector(0,0,Z);};
 I::SoftBox(Solid,TEXT("SD_FurnitureOakDark"),P(0,0,73),{166,82,6},DR,1.f);
 I::SoftBox(Solid,TEXT("SD_FurnitureOak"),P(0,0,76),{163,79,.7f},DR,.12f);
 for(float U:{-60.f,60.f}){
  I::SoftBox(Solid,TEXT("SD_FurnitureOakDark"),P(U,0,36),{40,70,70},DR,.7f);
  I::SoftBox(Solid,TEXT("SD_FurnitureOakDark"),P(U,0,3),{43,72,6},DR,.5f);
  for(int32 J=0;J<3;++J){const float Z=14+J*21;I::SoftBox(Solid,TEXT("SD_FurnitureOak"),P(U,-35.7f,Z),{36,2,18},DR,.5f);
   for(float S:{-1.f,1.f})Detail.Cylinder(TEXT("SD_CoachBrass"),P(U+S*6,-37,Z+1),P(U+S*6,-39.5f,Z+1),.7f,8,FHallUV::World(100));
   Detail.Cylinder(TEXT("SD_CoachBrass"),P(U-6,-39.5f,Z+1),P(U+6,-39.5f,Z+1),.85f,10,FHallUV::World(100));}
  // Shallow front frames and panels make the visitor-facing side a piece of furniture, rather than a solid block.
  I::SoftBox(Detail,TEXT("SD_FurnitureOak"),P(U,35.4f,38),{30,.8f,53},DR,.4f);
  for(float S:{-1.f,1.f})I::Box(Detail,TEXT("SD_WoodEdge"),P(U+S*16.5f,36,38),{1,.4f,57},DR);
 }
 I::SoftBox(Solid,TEXT("SD_FurnitureOak"),P(0,-1,64),{78,68,12},DR,.6f);
 I::SoftBox(Solid,TEXT("SD_FurnitureOakDark"),P(0,34,37),{82,3,60},DR,.5f);
 // Desk blotter, carbon-copy roster, pencil, thermos and a generic rotary telephone. No modern brands or equipment.
 I::SoftBox(Detail,TEXT("SD_CaseLeather"),P(3,5,77),{72,44,.55f},DR+FRotator(0,-6,0),.1f);
 C::Paper(Detail,TEXT("SD_CoachManila"),P(-4,4,77.35f),DX,DY,FVector::UpVector,35,39,-1,-2,.12f);
 C::Paper(Detail,TEXT("SD_CoachManila"),P(29,6,77.39f),DX,DY,FVector::UpVector,32,39,-1,2,.18f);
 Detail.Cylinder(TEXT("SD_FurnitureOakDark"),P(13,-15,77.43f),P(13,24,77.43f),.08f,5,FHallUV::World(100));
 C::Paper(Detail,TEXT("SD_CoachRoster"),P(-9,1,77.52f),DX,DY,FVector::UpVector,27,35,-1,-7,.20f);
 const float PageX[]={0,1.4f,-.8f,2.1f,-1.2f,.6f},PageY[]={0,-.9f,1.1f,-1.5f,.4f,1.6f},PageYaw[]={-9,3,-4,11,-1,6};
 for(int32 J=0;J<6;++J)I::Box(Detail,TEXT("SD_CoachPaper"),P(35+PageX[J],13+PageY[J],77.65f+J*.09f),{22,29,.10f},DR+FRotator(0,PageYaw[J],0));
 Detail.Cylinder(TEXT("SD_RoomPipe"),P(-21,-15,77.8f),P(-8,-11,77.8f),.32f,8,FHallUV::World(100));
 const FVector Flask=P(-55,18,76.4f);HouseCampusFallKit::Lathe(Solid,TEXT("SD_RoomPipe"),Flask,{{0,0},{4.5f,0},{4.7f,2},{4.7f,17},{3.6f,18},{3.6f,21},{0,21}},24);Solid.Cylinder(TEXT("SD_Dark"),Flask+FVector(0,0,21),Flask+FVector(0,0,25),4,20,FHallUV::World(100));
 const FRotator PR=DR+FRotator(0,-13,0);const FVector PX=PR.Vector(),PY(-PX.Y,PX.X,0);
 const FVector Phone=P(60,-14,76.5f);I::RotaryPhone(Solid,Detail,Phone,PX,PY);
 // Rubbed varnish, interrupted dark water rings and small cross-grain cuts where the desk was used.
 FRandomStream Wear(19861018);for(float Side:{-1.f,1.f})for(int32 J=0;J<25;++J){const float U=-78+J*6.1f,Width=Wear.FRandRange(.5f,2.5f);Detail.Quad(TEXT("SD_FurnitureOakRubbed"),P(U,Side*38.7f,76.42f),P(U+5.5f,Side*38.7f,76.42f),P(U+5.1f,Side*(38.7f-Width*.6f),76.42f),P(U+.4f,Side*(38.7f-Width),76.42f),FVector::UpVector,FHallUV::World(60));}
 for(FVector2D Centre:{FVector2D(-51,8),FVector2D(28,-20)}){const FVector Ring=P(Centre.X,Centre.Y,76.46f);for(int32 J=0;J<40;++J)if(J%9>1){const float A0=J*2*PI/40,A1=(J+1)*2*PI/40;Detail.Cylinder(TEXT("SD_LeatherCrack"),Ring+DX*(FMath::Cos(A0)*4.4f)+DY*(FMath::Sin(A0)*4.4f),Ring+DX*(FMath::Cos(A1)*4.4f)+DY*(FMath::Sin(A1)*4.4f),.11f,5,FHallUV::World(100));}}
 for(int32 J=0;J<19;++J){const float U=Wear.FRandRange(-75,75),V=Wear.FRandRange(-33,33),Span=Wear.FRandRange(1.5f,5.f);Detail.Cylinder(TEXT("SD_LeatherCrack"),P(U,V,76.44f),P(U+Span,V+.4f,76.44f),.065f,4,FHallUV::World(100));}
 // A green-shaded desk lamp uses emission only, sharing the existing six-light interior pool.
 const FRotator LR=DR+FRotator(0,14,0);const FVector LX=LR.Vector(),LY(-LX.Y,LX.X,0);
 const FVector Lamp=P(-63,-23,76.4f);Solid.Cylinder(TEXT("SD_CoachBrass"),Lamp,Lamp+FVector(0,0,1.5f),7,20,FHallUV::World(100));Solid.Cylinder(TEXT("SD_CoachBrass"),Lamp+FVector(0,0,1.5f),Lamp+FVector(0,0,29),.8f,10,FHallUV::World(100));
 // Pressed curved shade rather than the temporary rectangular proxy.
 for(int32 J=0;J<16;++J){const float T0=J*PI/16,T1=(J+1)*PI/16;auto V=[&](float U,float T){return Lamp+LX*U+LY*(FMath::Cos(T)*7)+FVector(0,0,28+FMath::Sin(T)*7);};Solid.Quad(TEXT("SD_CoachEnamel"),V(-13,T0),V(13,T0),V(13,T1),V(-13,T1),LY*FMath::Cos((T0+T1)*.5f)+FVector::UpVector*FMath::Sin((T0+T1)*.5f),FHallUV::World(100));for(float Side:{-1.f,1.f})HouseCampusFallKit::Tri(Solid,TEXT("SD_CoachEnamel"),Lamp+LX*(Side*13)+FVector(0,0,28),V(Side*13,T0),V(Side*13,T1),LX*Side,FHallUV::World(100));}
I::SoftBox(Glow,TEXT("SD_RoomTubeOld"),Lamp+FVector(0,0,26.5f),{24,10,.3f},LR,.05f);
 // Half-finished coffee, an addressed envelope, scrawled index cards, loose pens, paper clips and keys.
 const FVector Mug=P(-35,6,76.4f);HouseCampusFallKit::Lathe(Solid,TEXT("SD_Cream"),Mug,{{0,0},{3.5f,0},{4.1f,1},{4.2f,10.8f},{4.5f,11.6f},{3.7f,11.6f},{3.5f,1.4f},{0,1.4f}},24);
 Detail.Cylinder(TEXT("SD_LeatherCrack"),Mug+FVector(0,0,10.2f),Mug+FVector(0,0,10.3f),3.6f,24,FHallUV::World(100));
 for(int32 J=0;J<24;++J){auto Q=[&](int32 K){const float Angle=K*2*PI/24;return Mug+DX*(5.1f+FMath::Cos(Angle)*2.2f)+FVector(0,0,6+FMath::Sin(Angle)*3.4f);};Solid.Cylinder(TEXT("SD_Cream"),Q(J),Q(J+1),.6f,8,FHallUV::World(100));}
 I::Word(Detail,TEXT("CSU"),Mug+DY*4.3f+FVector(0,0,5),DX,DY,1.6f,TEXT("SD_LeatherCrack"));
 C::Paper(Detail,TEXT("SD_CoachClippings"),P(56,29,76.58f),DX,DY,FVector::UpVector,22,10,8,8,.20f);
 C::Paper(Detail,TEXT("SD_CoachClippings"),P(3,-25,76.57f),DX,DY,FVector::UpVector,14,9,1,-16,.12f);
 C::Paper(Detail,TEXT("SD_CoachClippings"),P(-15,25,77.55f),DX,DY,FVector::UpVector,12,8,2,13,.15f);
 Detail.Cylinder(TEXT("SD_Fabric"),P(23,-27,76.62f),P(35,-23,76.62f),.34f,8,FHallUV::World(100));Detail.Cylinder(TEXT("SD_CoachBrass"),P(34,-23.3f,76.62f),P(36,-22.7f,76.62f),.23f,8,FHallUV::World(100));
 Detail.Cylinder(TEXT("SD_Cream"),P(45,1,78.35f),P(49,12,78.35f),.30f,8,FHallUV::World(100));
 for(int32 J=0;J<3;++J){const FVector Clip=P(48+J*2.4f,-22+(J%2)*2.5f,76.55f);const float Angle=J*.7f;const FVector U=DX*FMath::Cos(Angle)+DY*FMath::Sin(Angle),V=DY*FMath::Cos(Angle)-DX*FMath::Sin(Angle);const FVector2D Wire[]={{-.4f,-.9f},{-.4f,.7f},{-.2f,1},{.3f,1},{.5f,.7f},{.5f,-.9f},{.2f,-1.1f},{-.1f,-1.1f},{-.2f,-.8f},{-.2f,.5f}};for(int32 K=1;K<UE_ARRAY_COUNT(Wire);++K)Detail.Cylinder(TEXT("SD_Aluminium"),Clip+U*Wire[K-1].X+V*Wire[K-1].Y,Clip+U*Wire[K].X+V*Wire[K].Y,.065f,5,FHallUV::World(100));}
 const FVector Keys=P(-16,-29,76.55f);for(int32 J=0;J<20;++J){const float A0=J*2*PI/20,A1=(J+1)*2*PI/20;Detail.Cylinder(TEXT("SD_CoachBrass"),Keys+DX*(FMath::Cos(A0)*1.5f)+DY*(FMath::Sin(A0)*1.5f),Keys+DX*(FMath::Cos(A1)*1.5f)+DY*(FMath::Sin(A1)*1.5f),.12f,6,FHallUV::World(100));}
 for(int32 J=0;J<2;++J){const FVector Tip=Keys+DX*(J?3.7f:3.1f)+DY*(J?2.8f:-2.1f);Detail.Cylinder(TEXT("SD_CoachBrass"),Keys,Tip,.22f,8,FHallUV::World(100));I::Box(Detail,TEXT("SD_CoachBrass"),Tip,{1.4f,.8f,.18f},DR+FRotator(0,J?31:-27,0));}
 HouseCampusFallKit::Ball(Detail,TEXT("SD_CoachPaper"),P(70,25,78.7f),2.3f,7);
 // Upholstered swivel chair: curved cushions, a slightly reclined back and compressed, uneven padding.
 const FRotator CR=R+FRotator(0,-12,0);const FVector CX=CR.Vector(),CY(-CX.Y,CX.X,0);
 const FVector Chair=CoachPoint(185,-264,-55),ChairZ=FVector::UpVector;
 Solid.Cylinder(TEXT("SD_Steel"),Chair+ChairZ*7,Chair+ChairZ*39,2.3f,20,FHallUV::World(100));
 I::SoftBox(Solid,TEXT("SD_Dark"),Chair+ChairZ*36,{23,17,4},CR,.7f);
 for(float U:{-1.f,1.f})for(float V:{-1.f,1.f}){const FVector Foot=Chair+CX*(U*22)+CY*(V*22)+ChairZ*6;Solid.Cylinder(TEXT("SD_Steel"),Chair+ChairZ*12,Foot,1.8f,12,FHallUV::World(100));Solid.Cylinder(TEXT("SD_Dark"),Foot-CX*2,Foot+CX*2,3.8f,16,FHallUV::World(100));}
 I::Cushion(Solid,Detail,Chair+ChairZ*43,CX,CY,ChairZ,49,46,11,false);
 const float Lean=12*PI/180.f;const FVector BackUp=ChairZ*FMath::Cos(Lean)-CY*FMath::Sin(Lean),ChairBackN=CY*FMath::Cos(Lean)+ChairZ*FMath::Sin(Lean),Back=Chair-CY*22+ChairZ*73;
 I::Cushion(Solid,Detail,Back,CX,BackUp,ChairBackN,46,43,11,true);
 for(float S:{-1.f,1.f}){Solid.Cylinder(TEXT("SD_Steel"),Chair+CX*(S*17)-CY*19+ChairZ*37,Chair+CX*(S*17)-CY*29+ChairZ*88,1.4f,12,FHallUV::World(100));Solid.Cylinder(TEXT("SD_Steel"),Chair+CX*(S*25)+CY*10+ChairZ*38,Chair+CX*(S*25)+CY*8+ChairZ*61,1.2f,12,FHallUV::World(100));I::Cushion(Solid,Detail,Chair+CX*(S*25)-CY*2+ChairZ*63,CX,CY,ChairZ,6,35,4,false);}
 // A small tilt handle and rubbed stitching make the mechanism legible without a modern office-chair silhouette.
 Solid.Cylinder(TEXT("SD_Steel"),Chair+CX*8+ChairZ*36,Chair+CX*23-CY*5+ChairZ*34,.7f,10,FHallUV::World(100));I::SoftBox(Solid,TEXT("SD_PhoneBakelite"),Chair+CX*24-CY*5+ChairZ*34,{6,3,2},CR,.6f);
 for(float S:{-1.f,1.f})Detail.Cylinder(TEXT("SD_PaddingDark"),Back+CX*(S*9)+ChairBackN*4.1f,Back+CX*(S*9)+ChairBackN*4.6f,.65f,16,FHallUV::World(100));
 // The visitor chair uses the same soft, rounded upholstery on its older timber frame.
 const FVector Guest=CoachPoint(-69,-135,-55);const FRotator GR=R+FRotator(0,-101,0);const FVector GX=GR.Vector(),GY(-GX.Y,GX.X,0);
 for(float U:{-19.f,19.f})for(float V:{-18.f,18.f})I::SoftBox(Solid,TEXT("SD_FurnitureOakDark"),Guest+GX*U+GY*V+ChairZ*21,{4,4,42},GR,.5f);
 I::Cushion(Solid,Detail,Guest+ChairZ*44,GX,GY,ChairZ,45,43,8,false);
 for(float U:{-19.f,19.f})I::SoftBox(Solid,TEXT("SD_FurnitureOakDark"),Guest+GX*U-GY*21+ChairZ*65,{4,4,48},GR,.4f);
 const float GuestLean=8*PI/180.f;I::Cushion(Solid,Detail,Guest-GY*21+ChairZ*72,GX,ChairZ*FMath::Cos(GuestLean)-GY*FMath::Sin(GuestLean),GY*FMath::Cos(GuestLean)+ChairZ*FMath::Sin(GuestLean),39,28,7,true);
 // Old steel filing cabinet: distinct drawer seams, inset label holders and wire handles.
 const FVector File=CoachPoint(270,-98,-55);I::SoftBox(Solid,TEXT("SD_CoachEnamel"),File+FVector(0,0,63),{43,60,126},R,.8f);
 for(int32 J=0;J<4;++J){const float Z=17+J*29;I::SoftBox(Detail,TEXT("SD_CoachEnamel"),File+Y*30.7f+FVector(0,0,Z),{38,1,26},R,.4f);I::Box(Detail,TEXT("SD_CoachBrass"),File+Y*31.3f+FVector(0,0,Z+5),{13,.6f,6},R);I::Box(Detail,TEXT("SD_CoachPaper"),File+Y*31.7f+FVector(0,0,Z+5),{11,.3f,4},R);for(float U:{-6.f,6.f})Detail.Cylinder(TEXT("SD_Aluminium"),File+X*U+Y*32+FVector(0,0,Z-5),File+X*U+Y*35+FVector(0,0,Z-5),.55f,8,FHallUV::World(100));Detail.Cylinder(TEXT("SD_Aluminium"),File-X*6+Y*35+FVector(0,0,Z-5),File+X*6+Y*35+FVector(0,0,Z-5),.7f,10,FHallUV::World(100));}
 const TCHAR* Files[]={TEXT("BILLS"),TEXT("1984"),TEXT("1985"),TEXT("ROSTER")};for(int32 J=0;J<4;++J)I::Word(Detail,Files[J],File+Y*32+FVector(0,0,19.8f+J*29),X,Y,2.f,TEXT("SD_LeatherCrack"));
 for(float Side:{-1.f,1.f})for(int32 J=0;J<9;++J){const float Z=8+J*13;const FVector Q=File+X*(Side*20.5f)+Y*30.85f+FVector(0,0,Z);HouseCampusFallKit::Tri(Detail,TEXT("SD_Rust"),Q-X*.5f,Q+X*.6f+FVector(0,0,1),Q+FVector(0,0,3.2f),Y,FHallUV::World(100));}
 // Low timber bookcase with mismatched binders; the back wall remains visible around it.
 const FVector Shelf=CoachPoint(30,-270,-55);for(float U:{-48.f,48.f})I::SoftBox(Solid,TEXT("SD_FurnitureOakDark"),Shelf+X*U+FVector(0,0,55),{3,26,110},R,.4f);
 I::Box(Solid,TEXT("SD_FurnitureOak"),Shelf-Y*12+FVector(0,0,55),{96,2,110},R);
 for(float Z:{3.f,38.f,73.f,109.f})I::SoftBox(Solid,TEXT("SD_FurnitureOak"),Shelf+FVector(0,0,Z),{96,26,3},R,.4f);
 for(int32 J=0;J<11;++J){const float U=-39+J*7.2f,H=22+(J%3)*3;I::SoftBox(Detail,J%3==0?TEXT("SD_CaseLeather"):J%3==1?TEXT("SD_Fabric"):TEXT("SD_Cream"),Shelf+X*U+FVector(0,0,40+H*.5f),{5+(J%2),20,H},R,.25f);I::Box(Detail,TEXT("SD_CoachBrass"),Shelf+X*U+Y*10.3f+FVector(0,0,51),{3,.2f,1},R);}
 // Move the authored newspaper and coach's notes into the separate room.
 const FVector BackX=(B-A).GetSafeNormal(),BackN(-BackX.Y,BackX.X,0),Board=C::Point(45,.768f,89)+BackN*9;
 I::SoftBox(Solid,TEXT("SD_FurnitureOakDark"),Board,{118,5,100},BackX.Rotation(),.7f);HouseCampusFallKit::Card(Detail,TEXT("SD_RoomNotices"),Board+BackN*2.7f,BackX,FVector::UpVector,110,92,BackN);
 for(FVector2D Q:{FVector2D(-48,40),FVector2D(1,40),FVector2D(44,32),FVector2D(-45,-12),FVector2D(-5,-10),FVector2D(43,-12)}){const FVector Pin=Board+BackN*3+BackX*Q.X+FVector(0,0,Q.Y);Detail.Cylinder(TEXT("SD_Rust"),Pin,Pin+BackN*.5f,.7f,8,FHallUV::World(100));}
 // Owner-requested bulletin-board clutter: deliberately overlapped and pinned at different angles.
 C::Paper(Detail,TEXT("SD_CoachSnapshot"),Board-BackX*15+FVector(0,0,20)+BackN*3.6f,BackX,FVector::UpVector,BackN,40,26,-1,-5,.25f);
 struct FNote{float U,V,W,H,Angle;int32 Cell;};const FNote Notes[]={{34,23,20,23,-7,1},{5,-35,28,10,11,0},{-40,-22,20,15,8,2},{11,-9,22,28,-5,3},{38,-19,18,18,12,5},{-14,-15,23,10,-13,6},{-40,26,18,22,-9,4},{28,1,17,12,6,8}};
 int32 Sheet=0;for(const FNote& Note:Notes){const FVector Centre=Board+BackX*Note.U+FVector(0,0,Note.V)+BackN*(3.9f+Sheet*.11f);C::Paper(Detail,TEXT("SD_CoachClippings"),Centre,BackX,FVector::UpVector,BackN,Note.W,Note.H,Note.Cell,Note.Angle,.30f+(Sheet%3)*.15f);const float A0=FMath::DegreesToRadians(Note.Angle);const FVector Up=FVector::UpVector*FMath::Cos(A0)-BackX*FMath::Sin(A0),Pin=Centre+Up*(Note.H*.5f-1)+BackN*.15f;Detail.Cylinder(Sheet%3==0?TEXT("SD_CoachBrass"):TEXT("SD_Rust"),Pin,Pin+BackN*.5f,.65f,10,FHallUV::World(100));++Sheet;}
 for(float U:{-30.f,1.f}){const FVector Pin=Board+BackX*U+FVector(0,0,30)+BackN*4;Detail.Cylinder(TEXT("SD_CoachBrass"),Pin,Pin+BackN*.6f,.65f,10,FHallUV::World(100));}
 // Personal wall pieces sit unevenly on the existing plaster, clear of the door and all established walking lanes.
 const FVector InnerAlong=(A-D).GetSafeNormal(),InnerN=FVector::CrossProduct(FVector::UpVector,InnerAlong).GetSafeNormal();
 auto Frame=[&](FVector Centre,float Width,float Height,float Angle,int32 Cell){const float T=Angle*PI/180.f;const FVector Across=InnerAlong*FMath::Cos(T)+FVector::UpVector*FMath::Sin(T),Rise=FVector::UpVector*FMath::Cos(T)-InnerAlong*FMath::Sin(T);HouseCampusFallKit::Card(Detail,TEXT("SD_CoachPaper"),Centre,Across,Rise,Width,Height,InnerN);C::Paper(Detail,TEXT("SD_CoachWallDetails"),Centre+InnerN*.15f,Across,Rise,InnerN,Width-5,Height-5,Cell,0,0,2);for(float S:{-1.f,1.f}){HouseCampusFallKit::Bar(Solid,TEXT("SD_FurnitureOakDark"),Centre+Across*(S*Width*.5f)-Rise*Height*.5f,Centre+Across*(S*Width*.5f)+Rise*Height*.5f,2.8f,2.1f,InnerN);HouseCampusFallKit::Bar(Solid,TEXT("SD_FurnitureOakDark"),Centre+Rise*(S*Height*.5f)-Across*Width*.5f,Centre+Rise*(S*Height*.5f)+Across*Width*.5f,2.8f,2.1f,InnerN);}const FVector Nail=Centre+FVector(0,0,Height*.5f+8)-InnerN*.3f;for(float S:{-1.f,1.f})Detail.Cylinder(TEXT("SD_Rust"),Nail,Centre+Across*(S*Width*.35f)+Rise*Height*.35f-InnerN*.3f,.08f,6,FHallUV::World(100));I::Screw(Detail,Nail,InnerN,.3f);};
 Frame(FMath::Lerp(A,D,.72f)+FVector(0,0,164)+InnerN*8,56,40,-2.5f,0);
 const FVector Calendar=FMath::Lerp(A,D,.37f)+FVector(0,0,141)+InnerN*8;
 C::Paper(Detail,TEXT("SD_CoachPaper"),Calendar-InnerN*.2f,InnerAlong,FVector::UpVector,InnerN,38,54,-1,2,.6f);
 C::Paper(Detail,TEXT("SD_CoachWallDetails"),Calendar,InnerAlong,FVector::UpVector,InnerN,38,54,1,2,1.1f,2);
 for(float Side:{-1.f,1.f})Detail.Cylinder(TEXT("SD_PhoneMetal"),Calendar+InnerAlong*(Side*10)+FVector(0,0,27),Calendar+InnerAlong*(Side*10)+FVector(0,0,25)+InnerN*.4f,.30f,8,FHallUV::World(100));
 const FVector Pennant=C::Point(45,.735f,123)+BackN*8.8f;const FVector Tip=Pennant+BackX*40-FVector(0,0,3),Top=Pennant-BackX*40+FVector(0,0,18),Bottom=Pennant-BackX*40-FVector(0,0,18);
 {auto& S=Detail.Sec(TEXT("SD_CoachWallDetails"));const int32 First=S.V.Num();HouseCampusFallKit::Tri(Detail,TEXT("SD_CoachWallDetails"),Bottom,Tip,Top,BackN);S.UV[First]={.50f,1};S.UV[First+1]={1,.75f};S.UV[First+2]={.50f,.50f};for(int32 J=First;J<First+3;++J)S.T[J]=FProcMeshTangent(BackX,false);}
 Detail.Cylinder(TEXT("SD_FabricEdge"),Bottom+BackN*.1f,Tip+BackN*.1f,.16f,6,FHallUV::World(100));Detail.Cylinder(TEXT("SD_FabricEdge"),Top+BackN*.1f,Tip+BackN*.1f,.16f,6,FHallUV::World(100));
 for(FVector Pin:{Top,Bottom})I::Screw(Detail,Pin+BackN*.2f,BackN,.36f);
 // Brass doorplate is beside the opening at reading height, clear of the ceiling ribs.
 const FVector Label=CoachPoint(-102,24,112);I::SoftBox(Solid,TEXT("SD_FurnitureOakDark"),Label,{72,2,25},R,.6f);I::Word(Detail,TEXT("COACH"),Label+Y*1.2f+FVector(0,0,-4),X,Y,9);
 UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_COACH_OFFICE built=1 furniture=desk,swivel,visitor,files,bookcase connected=1 newLights=0"));
}
