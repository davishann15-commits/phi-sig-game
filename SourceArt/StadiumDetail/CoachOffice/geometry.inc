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
 double Sign=0;for(int32 J=0;J<4;++J){const FVector A=Poly[J],B=Poly[(J+1)%4];const double Cross=(B.X-A.X)*(P.Y-A.Y)-(B.Y-A.Y)*(P.X-A.X);if(J==0)Sign=Cross;else if(Sign*Cross<0)return false;}return true;
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
  I::SoftBox(Solid,TEXT("SD_LockerOakDark"),Mid+FVector(0,0,5),{L,16,10},U.Rotation(),.4f);
  for(float Z:{100.f,226.f})I::SoftBox(Detail,TEXT("SD_LockerOakDark"),Mid+FVector(0,0,Z),{L,14,3},U.Rotation(),.3f);
  (void)N;
 }
 RoomFixture(Glow,Detail,CoachPoint(100,-140,170),X);
 // A broad 1930s oak pedestal desk: knee space, separate drawer fronts, brass pulls, recessed panels and worn top edges.
 const FVector Desk=CoachPoint(145,-164,-55);auto P=[&](float U,float V,float Z){return Desk+X*U+Y*V+FVector(0,0,Z);};
 I::SoftBox(Solid,TEXT("SD_LockerOakDark"),P(0,0,73),{166,82,6},R,1.f);
 I::SoftBox(Solid,TEXT("SD_BenchWood"),P(0,0,76),{163,79,.7f},R,.12f);
 for(float U:{-60.f,60.f}){
  I::SoftBox(Solid,TEXT("SD_LockerOakDark"),P(U,0,36),{40,70,70},R,.7f);
  I::SoftBox(Solid,TEXT("SD_LockerOakDark"),P(U,0,3),{43,72,6},R,.5f);
  for(int32 J=0;J<3;++J){const float Z=14+J*21;I::SoftBox(Solid,TEXT("SD_LockerOak"),P(U,-35.7f,Z),{36,2,18},R,.5f);
   for(float S:{-1.f,1.f})Detail.Cylinder(TEXT("SD_NameBrass"),P(U+S*6,-37,Z+1),P(U+S*6,-39.5f,Z+1),.7f,8,FHallUV::World(100));
   Detail.Cylinder(TEXT("SD_NameBrass"),P(U-6,-39.5f,Z+1),P(U+6,-39.5f,Z+1),.85f,10,FHallUV::World(100));}
  // Shallow front frames and panels make the visitor-facing side a piece of furniture, rather than a solid block.
  I::SoftBox(Detail,TEXT("SD_LockerOak"),P(U,35.4f,38),{30,.8f,53},R,.4f);
  for(float S:{-1.f,1.f})I::Box(Detail,TEXT("SD_WoodEdge"),P(U+S*16.5f,36,38),{1,.4f,57},R);
 }
 I::SoftBox(Solid,TEXT("SD_LockerOak"),P(0,-1,64),{78,68,12},R,.6f);
 I::SoftBox(Solid,TEXT("SD_LockerOakDark"),P(0,34,37),{82,3,60},R,.5f);
 // Desk blotter, carbon-copy roster, pencil, thermos and a generic rotary telephone. No modern brands or equipment.
 I::SoftBox(Detail,TEXT("SD_CaseLeather"),P(0,2,77),{72,44,.55f},R,.1f);
 HouseCampusFallKit::Card(Detail,TEXT("SD_CoachRoster"),P(-8,1,77.4f),X,Y,27,35,FVector::UpVector);
 for(int32 J=0;J<4;++J)I::Box(Detail,TEXT("SD_Cream"),P(37+J*.4f,13-J*.5f,77.3f+J*.14f),{22,29,.12f},R);
 Detail.Cylinder(TEXT("SD_RoomPipe"),P(-21,-15,77.8f),P(-8,-11,77.8f),.32f,8,FHallUV::World(100));
 const FVector Flask=P(-62,13,76.4f);HouseCampusFallKit::Lathe(Detail,TEXT("SD_RoomPipe"),Flask,{{0,0},{4.5f,0},{4.7f,2},{4.7f,17},{3.6f,18},{3.6f,21},{0,21}},24);Detail.Cylinder(TEXT("SD_Dark"),Flask+FVector(0,0,21),Flask+FVector(0,0,25),4,20,FHallUV::World(100));
 const FVector Phone=P(58,-12,76.5f);I::SoftBox(Detail,TEXT("SD_CaseLeather"),Phone+FVector(0,0,3),{22,23,6},R,1.f);
 Detail.Cylinder(TEXT("SD_NameBrass"),Phone+FVector(0,0,6.1f),Phone+FVector(0,0,6.4f),6,24,FHallUV::World(100));
 for(int32 J=0;J<10;++J){const float T=J*2*PI/10;const FVector Q=Phone+X*(FMath::Cos(T)*4.3f)+Y*(FMath::Sin(T)*4.3f)+FVector(0,0,6.5f);Detail.Cylinder(TEXT("SD_Dark"),Q,Q+FVector(0,0,.18f),.75f,8,FHallUV::World(100));}
 for(float S:{-1.f,1.f})I::SoftBox(Detail,TEXT("SD_CaseLeather"),Phone+X*(S*8)+Y*8+FVector(0,0,9),{7,8,5},R,1.f);
 Detail.Cylinder(TEXT("SD_CaseLeather"),Phone-X*8+Y*8+FVector(0,0,11),Phone+X*8+Y*8+FVector(0,0,11),1.7f,12,FHallUV::World(100));
 for(int32 J=0;J<85;++J){auto Q=[&](int32 K){const float T=K/84.f;return Phone+X*(12+FMath::Sin(T*PI)*9)+Y*(8-T*31)+FVector(0,0,5+FMath::Sin(T*18*PI)*.6f);};Detail.Cylinder(TEXT("SD_Dark"),Q(J),Q(J+1),.25f,5,FHallUV::World(100));}
 // A green-shaded desk lamp uses emission only, sharing the existing six-light interior pool.
 const FVector Lamp=P(-58,-23,76.4f);Detail.Cylinder(TEXT("SD_NameBrass"),Lamp,Lamp+FVector(0,0,1.5f),7,20,FHallUV::World(100));Detail.Cylinder(TEXT("SD_NameBrass"),Lamp+FVector(0,0,1.5f),Lamp+FVector(0,0,29),.8f,10,FHallUV::World(100));
 I::SoftBox(Detail,TEXT("SD_CoachEnamel"),Lamp+FVector(0,0,30),{27,13,7},R,2.f);I::SoftBox(Glow,TEXT("SD_RoomTubeOld"),Lamp+FVector(0,0,26.5f),{24,10,.3f},R,.05f);
 // Four-spoke upholstered swivel chair; rounded pads, pressed-steel frame and actual rubber casters.
 const FVector Chair=CoachPoint(180,-262,-55);
 Detail.Cylinder(TEXT("SD_Steel"),Chair+FVector(0,0,7),Chair+FVector(0,0,39),2.3f,16,FHallUV::World(100));
 for(float U:{-1.f,1.f})for(float V:{-1.f,1.f}){const FVector Foot=Chair+X*(U*22)+Y*(V*22)+FVector(0,0,6);Solid.Cylinder(TEXT("SD_Steel"),Chair+FVector(0,0,12),Foot,1.8f,10,FHallUV::World(100));Solid.Cylinder(TEXT("SD_Dark"),Foot-X*2,Foot+X*2,3.8f,12,FHallUV::World(100));}
 I::SoftBox(Solid,TEXT("SD_CoachUpholstery"),Chair+FVector(0,0,43),{49,46,9},R,1.8f);
 I::SoftBox(Solid,TEXT("SD_CoachUpholstery"),Chair-Y*21+FVector(0,0,73),{46,8,45},R,1.5f);
 for(float S:{-1.f,1.f}){Solid.Cylinder(TEXT("SD_Steel"),Chair+X*(S*17)-Y*19+FVector(0,0,37),Chair+X*(S*17)-Y*23+FVector(0,0,83),1.4f,10,FHallUV::World(100));Solid.Cylinder(TEXT("SD_Steel"),Chair+X*(S*25)+Y*10+FVector(0,0,38),Chair+X*(S*25)+Y*10+FVector(0,0,61),1.2f,10,FHallUV::World(100));I::SoftBox(Solid,TEXT("SD_LockerOakDark"),Chair+X*(S*25)+FVector(0,0,63),{5,39,4},R,.8f);}
 I::Piping(Detail,TEXT("SD_PaddingDark"),Chair,X,Y,23.5f,21.8f,46.7f,.22f);
 // Visitor chair sits against the inner wall, outside the direct doorway-to-desk lane.
 const FVector Guest=CoachPoint(-40,-150,-55),GX=Y,GY=-X;const FRotator GR=GX.Rotation();
 for(float U:{-19.f,19.f})for(float V:{-18.f,18.f})I::SoftBox(Solid,TEXT("SD_LockerOakDark"),Guest+GX*U+GY*V+FVector(0,0,21),{4,4,42},GR,.5f);
 I::SoftBox(Solid,TEXT("SD_CoachUpholstery"),Guest+FVector(0,0,44),{45,43,7},GR,1.5f);
 for(float U:{-19.f,19.f})I::SoftBox(Solid,TEXT("SD_LockerOakDark"),Guest+GX*U-GY*20+FVector(0,0,65),{4,4,48},GR,.4f);
 I::SoftBox(Solid,TEXT("SD_CoachUpholstery"),Guest-GY*20+FVector(0,0,72),{39,5,27},GR,1.f);
 // Old steel filing cabinet: distinct drawer seams, inset label holders and wire handles.
 const FVector File=CoachPoint(270,-98,-55);I::SoftBox(Solid,TEXT("SD_CoachEnamel"),File+FVector(0,0,63),{43,60,126},R,.8f);
 for(int32 J=0;J<4;++J){const float Z=17+J*29;I::SoftBox(Detail,TEXT("SD_RoomBeam"),File+Y*30.7f+FVector(0,0,Z),{38,1,26},R,.4f);I::Box(Detail,TEXT("SD_NameBrass"),File+Y*31.3f+FVector(0,0,Z+5),{13,.6f,6},R);I::Box(Detail,TEXT("SD_Cream"),File+Y*31.7f+FVector(0,0,Z+5),{11,.3f,4},R);for(float U:{-6.f,6.f})Detail.Cylinder(TEXT("SD_Aluminium"),File+X*U+Y*32+FVector(0,0,Z-5),File+X*U+Y*35+FVector(0,0,Z-5),.55f,8,FHallUV::World(100));Detail.Cylinder(TEXT("SD_Aluminium"),File-X*6+Y*35+FVector(0,0,Z-5),File+X*6+Y*35+FVector(0,0,Z-5),.7f,10,FHallUV::World(100));}
 // Low timber bookcase with mismatched binders; the back wall remains visible around it.
 const FVector Shelf=CoachPoint(30,-270,-55);for(float U:{-48.f,48.f})I::SoftBox(Solid,TEXT("SD_LockerOakDark"),Shelf+X*U+FVector(0,0,55),{3,26,110},R,.4f);
 I::Box(Solid,TEXT("SD_LockerOak"),Shelf-Y*12+FVector(0,0,55),{96,2,110},R);
 for(float Z:{3.f,38.f,73.f,109.f})I::SoftBox(Solid,TEXT("SD_LockerOak"),Shelf+FVector(0,0,Z),{96,26,3},R,.4f);
 for(int32 J=0;J<11;++J){const float U=-39+J*7.2f,H=22+(J%3)*3;I::SoftBox(Detail,J%3==0?TEXT("SD_CaseLeather"):J%3==1?TEXT("SD_Fabric"):TEXT("SD_Cream"),Shelf+X*U+FVector(0,0,40+H*.5f),{5+(J%2),20,H},R,.25f);I::Box(Detail,TEXT("SD_NameBrass"),Shelf+X*U+Y*10.3f+FVector(0,0,51),{3,.2f,1},R);}
 // Move the authored newspaper and coach's notes into the separate room.
 const FVector BackX=(B-A).GetSafeNormal(),BackN(-BackX.Y,BackX.X,0),Board=C::Point(45,.768f,89)+BackN*9;
 I::SoftBox(Solid,TEXT("SD_LockerOakDark"),Board,{118,5,100},BackX.Rotation(),.7f);HouseCampusFallKit::Card(Detail,TEXT("SD_RoomNotices"),Board+BackN*2.7f,BackX,FVector::UpVector,110,92,BackN);
 for(FVector2D Q:{FVector2D(-48,40),FVector2D(1,40),FVector2D(44,32),FVector2D(-45,-12),FVector2D(-5,-10),FVector2D(43,-12)}){const FVector Pin=Board+BackN*3+BackX*Q.X+FVector(0,0,Q.Y);Detail.Cylinder(TEXT("SD_Rust"),Pin,Pin+BackN*.5f,.7f,8,FHallUV::World(100));}
 // Brass doorplate is beside the opening at reading height, clear of the ceiling ribs.
 const FVector Label=CoachPoint(-102,24,112);I::SoftBox(Solid,TEXT("SD_LockerOakDark"),Label,{72,2,25},R,.6f);I::Word(Detail,TEXT("COACH"),Label+Y*1.2f+FVector(0,0,-4),X,Y,9);
 UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_COACH_OFFICE built=1 furniture=desk,swivel,visitor,files,bookcase connected=1 newLights=0"));
}
