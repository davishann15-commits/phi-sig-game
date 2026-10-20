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
 for(int32 J=0;J<3;++J)I::SoftBox(Solid,J==1?TEXT("SD_LockerOakLight"):TEXT("SD_LockerOak"),C+Facing*((J-1)*11.8f)+FVector(0,0,44),{Span,11.2f,4},R,.5f);
 for(float U=20;U<Span;U+=120){const FVector P=Start+Along*U;for(float S:{-1.f,1.f}){I::Box(Solid,TEXT("SD_Steel"),P+Facing*(S*12)+FVector(0,0,20),{4,4,40},R);I::Box(Solid,TEXT("SD_Steel"),P+Facing*(S*12)+FVector(0,0,1),{15,9,2},R);I::Screw(Detail,P+Facing*(S*13.9f)+FVector(0,0,38),Facing*S,.6f);}I::Box(Solid,TEXT("SD_Steel"),P+FVector(0,0,40),{7,35,3},R);Detail.Cylinder(TEXT("SD_Steel"),P-Along*20+FVector(0,0,20),P+FVector(0,0,40),1.5f,8,FHallUV::World(100));}
 I::Box(Solid,TEXT("SD_Steel"),C+FVector(0,0,18),{Span-30,3,3},R);
}
void HouseStadiumDetail::LockerHelmet(FHallMesh& Solid,FHallMesh& Detail,FVector Base,FVector Facing)
{
 namespace I=HouseStadiumDetailInteriorsImpl;const FVector X=Facing.GetSafeNormal2D(),Y(-X.Y,X.X,0);
 auto P=[&](float Xv,float Yv,float Z){return Base+X*Xv+Y*Yv+FVector(0,0,Z);};
 auto Shell=[&](float T,float A){return P(14*FMath::Sin(T)*FMath::Cos(A),11.7f*FMath::Sin(T)*FMath::Sin(A),16+15*FMath::Cos(T));};
 auto Limit=[](float A){return 2.24f-1.04f*FMath::Pow(FMath::Max(0.f,FMath::Cos(A)),3.f);};
 for(int32 J=0;J<32;++J){const float A=J*2*PI/32,B=(J+1)*2*PI/32;for(int32 K=0;K<12;++K){const float T0=(K+.01f)/12,T1=(K+1.f)/12;const FVector V[]={Shell(T0*Limit(A),A),Shell(T0*Limit(B),B),Shell(T1*Limit(B),B),Shell(T1*Limit(A),A)};const FVector N=((V[0]+V[1]+V[2]+V[3])*.25f-P(0,0,16)).GetSafeNormal();Solid.Quad(TEXT("SD_HelmetShell"),V[0],V[1],V[2],V[3],N,FHallUV::World(100));}
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
