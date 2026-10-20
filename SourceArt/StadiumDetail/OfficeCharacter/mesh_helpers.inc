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
 for(int32 Ring=0;Ring<4;++Ring)for(int32 K=0;K<32;++K){const FVector V[]={Rim(Ring,K),Rim(Ring,(K+1)%32),Rim(Ring+1,(K+1)%32),Rim(Ring+1,K)};Solid.Quad(Body,V[0],V[1],V[2],V[3],FVector::CrossProduct(V[1]-V[0],V[3]-V[0]).GetSafeNormal(),FHallUV::World(30));}
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
  const FVector Cup=P(Side*8.7f,8.1f,9.0f);const int32 First=Solid.Sec(Body).V.Num(),FirstIndex=Solid.Sec(Body).I.Num();HouseCampusFallKit::Lathe(Solid,Body,FVector::ZeroVector,{{0,0},{3.6f,0},{3.9f,.7f},{3.8f,1.5f},{3.3f,2.2f},{2.45f,4.8f},{2.45f,5.7f},{0,5.7f}},32);auto& S=Solid.Sec(Body);const FQuat Tilt=FQuat::FindBetweenNormals(Z,(Z+X*(Side*.13f)).GetSafeNormal());for(int32 J=First;J<S.V.Num();++J){S.V[J]=Cup+Tilt.RotateVector(S.V[J]);S.N[J]=-Tilt.RotateVector(S.N[J]);S.T[J].TangentX=Tilt.RotateVector(S.T[J].TangentX);}for(int32 J=FirstIndex;J<S.I.Num();J+=3)Swap(S.I[J+1],S.I[J+2]);
  for(int32 K=0;K<32;++K){const float A=K*2*PI/32,B=(K+1)*2*PI/32;Detail.Cylinder(TEXT("SD_Dark"),Cup+X*(FMath::Cos(A)*3.86f)+Y*(FMath::Sin(A)*3.86f)+Z*.75f,Cup+X*(FMath::Cos(B)*3.86f)+Y*(FMath::Sin(B)*3.86f)+Z*.75f,.055f,5,FHallUV::World(100));}
 }
 auto Grip=[&](float T){return P(-8.7f+17.4f*T,8.1f,13.6f+2.2f*FMath::Sin(T*PI));};
 for(int32 J=0;J<24;++J){const float T0=J/24.f,T1=(J+1)/24.f;const FVector Tangent=(Grip(FMath::Min(1.f,T0+.01f))-Grip(FMath::Max(0.f,T0-.01f))).GetSafeNormal(),V=FVector::CrossProduct(Tangent,Y).GetSafeNormal();for(int32 K=0;K<12;++K){const float A=K*2*PI/12,B=(K+1)*2*PI/12;const FVector D0=Y*(FMath::Cos(A)*1.7f)+V*(FMath::Sin(A)*1.6f),D1=Y*(FMath::Cos(B)*1.7f)+V*(FMath::Sin(B)*1.6f);Solid.Quad(Body,Grip(T0)+D0,Grip(T0)+D1,Grip(T1)+D1,Grip(T1)+D0,(D0+D1).GetSafeNormal(),FHallUV::World(30));}}
 for(int32 J=0;J<180;++J){auto Q=[&](int32 K){const float T=K/180.f,A=T*30*PI;return P(13+FMath::Sin(T*PI)*2.2f+FMath::Cos(A)*.65f,7-T*25,4.8f*(1-T)+.85f+FMath::Sin(A)*.65f);};Detail.Cylinder(Body,Q(J),Q(J+1),.19f,6,FHallUV::World(100));}
 Detail.Cylinder(Body,P(10.2f,8.1f,10.2f),P(13.65f,7,5.65f),.25f,8,FHallUV::World(100));
}
