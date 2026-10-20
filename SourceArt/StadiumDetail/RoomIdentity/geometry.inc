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
 const FVector Base=I::At(50.f,.735f,0)+Into*23;
 I::SoftBox(Solid,TEXT("SD_LockerOakDark"),Base+FVector(0,0,100),{118,5,100},Along.Rotation(),.7f);
 HouseCampusFallKit::Card(Detail,TEXT("SD_RoomNotices"),Base+Into*2.7f+FVector(0,0,100),Along,FVector::UpVector,110,92,Into);
 for(FVector2D Q:{FVector2D(-48,40),FVector2D(1,40),FVector2D(44,32),FVector2D(-45,-12),FVector2D(-5,-10),FVector2D(43,-12)}){const FVector P=Base+Into*3+Along*Q.X+FVector(0,0,100+Q.Y);Detail.Cylinder(TEXT("SD_Rust"),P,P+Into*.5f,.7f,8,FHallUV::World(100));}
 // Worn team stripe interrupts the old wall's scale, with cream plaster remaining the dominant surface.
 const FVector Stripe=I::At(50.f,.792f,133.f)+Into*22.2f;HouseCampusFallKit::Card(Detail,TEXT("SD_RoomStripe"),Stripe,Along,FVector::UpVector,900,13,Into);
 const FVector Pipe=I::At(50.f,.715f,0)+Into*23;Solid.Cylinder(TEXT("SD_RoomPipe"),Pipe+FVector(0,0,-38),Pipe+FVector(0,0,163),2.3f,12,FHallUV::World(100));
 const FVector Valve=Pipe+Into*4+FVector(0,0,34);Detail.Cylinder(TEXT("SD_Steel"),Valve-Into*5,Valve,1.1f,10,FHallUV::World(100));for(int32 J=0;J<20;++J){const float A=J*2*PI/20,B=(J+1)*2*PI/20;Detail.Cylinder(TEXT("SD_Rust"),Valve+Along*(FMath::Cos(A)*5)+FVector::UpVector*(FMath::Sin(A)*5),Valve+Along*(FMath::Cos(B)*5)+FVector::UpVector*(FMath::Sin(B)*5),.65f,6,FHallUV::World(100));}
 const FVector Desk=I::At(52.5f,.73f,-55.f);const FVector Flask=Desk+FVector(-34,-8,76);
 HouseCampusFallKit::Lathe(Detail,TEXT("SD_RoomPipe"),Flask,{{0,0},{4.5f,0},{4.7f,2},{4.7f,17},{3.6f,18},{3.6f,21},{0,21}},24);Detail.Cylinder(TEXT("SD_Dark"),Flask+FVector(0,0,21),Flask+FVector(0,0,25),4,20,FHallUV::World(100));
 Detail.Cylinder(TEXT("SD_Dark"),Desk+FVector(-4,8,76.5f),Desk+FVector(9,11,76.5f),.32f,8,FHallUV::World(100));
 // Two off-white towels settle into wrinkles at their original dressing positions, with no new collision footprint.
 for(float Angle:{54.f,61.5f}){const FVector C=I::At(Angle,.82f,-54.8f);auto P=[&](int32 X,int32 Y){const float U=X/10.f,V=Y/7.f,Envelope=FMath::Sin(U*PI)*FMath::Sin(V*PI);return C+FVector((U-.5f)*50,(V-.5f)*34,.12f+Envelope*(1.7f+1.2f*FMath::Sin(U*21+V*17)));};for(int32 X=0;X<10;++X)for(int32 Y=0;Y<7;++Y)Detail.Quad(TEXT("SD_Towel"),P(X,Y),P(X+1,Y),P(X+1,Y+1),P(X,Y+1),FVector::UpVector,FHallUV::World(60));}
}
