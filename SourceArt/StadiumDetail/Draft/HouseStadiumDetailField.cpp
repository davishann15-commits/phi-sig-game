#include "HouseStadiumDetail.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallPaint.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
void HouseStadiumDetail::FieldCollision(const FHallMesh& Visual,FHallMesh& Solid)
{
 if(!Enabled()||Step()<4)return;
 for(FName Key:{FName(TEXT("CF_Bench")),FName(TEXT("CF_Aluminium")),FName(TEXT("CF_Cooler")),FName(TEXT("CF_PostGold")),FName(TEXT("CF_PostPad")),FName(TEXT("CF_Pumpkin"))})
 {
  const FHallSection* Source=Visual.Sections.Find(Key);if(!Source)continue;FHallSection& Target=Solid.Sec(Key);const int32 Offset=Target.V.Num();Target.V.Append(Source->V);Target.N.Append(Source->N);Target.UV.Append(Source->UV);Target.T.Append(Source->T);for(int32 Index:Source->I)Target.I.Add(Index+Offset);
 }
 UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_FIELD retainedBenchTablesCoolersGoalpostsPylonsCollision=exactVisual paperCollision=0 grassCollision=0 newLights=0"));
}
void AHouseStadiumDetail::Field()
{
 FRandomStream Rand(19862417);constexpr float Turf=-56.f;
 // Short real blades at walking distance only; two conventional LODs, one shared batch, no grass actors/shadows.
 if(auto* Grass=HouseStadiumDetail::GrassKit(Look))
 {
  OwnedMeshes.Add(Grass);auto* H=NewObject<UHierarchicalInstancedStaticMeshComponent>(this,TEXT("CalderStadiumNearTurf"));AddInstanceComponent(H);H->SetupAttachment(RootComponent);H->SetMobility(EComponentMobility::Static);H->SetStaticMesh(Grass);H->SetCollisionEnabled(ECollisionEnabled::NoCollision);H->SetGenerateOverlapEvents(false);H->SetCanEverAffectNavigation(false);H->SetCastShadow(false);H->bAffectDistanceFieldLighting=false;H->bAffectDynamicIndirectLighting=false;H->SetCullDistances(1000,1500);H->bAutoRebuildTreeOnInstanceChanges=false;H->RegisterComponent();
  for(int32 X=-35;X<=35;++X)for(int32 Y=-23;Y<=23;++Y){const float PX=X*100.f,PY=Y*100.f;const float Radius=FMath::Sqrt(FMath::Square(PX/5600)+FMath::Square(PY/3800));if(Radius>.655f||FMath::Square(PX/410)+FMath::Square(PY/410)<1)continue;if(FMath::Abs(PY)<1400){if(FMath::Abs(PX)>2450&&FMath::Abs(PX)<3200)continue;const float YardLine=FMath::Fmod(PX+2514.5f+251.45f,251.45f);if(YardLine<45||YardLine>206)continue;if(FMath::Abs(PY)<450&&(X+Y)%3==0)continue;}H->AddInstance(FTransform(FRotator(0,Rand.FRandRange(0,360),0),FVector(2400+PX,-13300+PY,Turf+.02f)),false);}
  H->BuildTreeIfOutdated(false,true);UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_TURF patches=%d lodTriangles=160/32 cull=1500 groundZ=-56 collision=existingGround newLights=0"),H->GetInstanceCount());
 }
 // Divots are thin irregular soil/grass chips, not floating clods or a change to the fixed turf elevation.
 for(int32 I=0;I<150;++I){const FVector P(2400+Rand.FRandRange(-2350,2350),-13300+Rand.FRandRange(-440,440),Turf+.07f);Box(I%3?TEXT("SD_GrassWorn"):TEXT("SD_Dirt"),P,{Rand.FRandRange(6,20),Rand.FRandRange(3,10),.12f},FRotator(0,Rand.FRandRange(0,180),0),false,6500);}
 // Down-and-distance chains dropped in a lazy curve on the home sideline, away from a walking route.
 constexpr float ChainSpan=492.f,ChainCurve=12.f;const FVector Base(2780,-11455,Turf+1.9f);
 // Closely pitched orthogonal ovals pass through one another, with their lowest wire0.1cm above turf.
 // The gentle curve plus end links is about ten marked yards at the existing0.55 field scale.
 for(int32 I=0;I<90;++I){const float T=I/89.f;const FVector P=Base+FVector(T*ChainSpan,FMath::Sin(T*PI*2)*ChainCurve,0),Along=FVector(ChainSpan,FMath::Cos(T*PI*2)*ChainCurve*2*PI,0).GetSafeNormal(),Side=I%2?FVector::UpVector:FVector(-Along.Y,Along.X,0);for(int32 J=0;J<12;++J){const float A=J*PI/6,B=(J+1)*PI/6;Pipe(TEXT("SD_Aluminium"),P+Along*(FMath::Cos(A)*3.5f)+Side*(FMath::Sin(A)*1.5f),P+Along*(FMath::Cos(B)*3.5f)+Side*(FMath::Sin(B)*1.5f),.3f,false,6500);}}
 for(float X:{Base.X-20.f,Base.X+ChainSpan-20.f}){Pipe(TEXT("SD_Orange"),{X,-11472,Turf+9},{X+230,-11425,Turf+9},5,true,20000);Box(TEXT("SD_Orange"),{X,-11472,Turf+10},{50,36,18},FRotator(0,12,0));}
 Box(TEXT("SD_Dark"),{2980,-11410,Turf+7.5f},{64,15,50},FRotator(0,0,90));Unique.QuadUV(TEXT("SD_DownBoard"),{2949,-11434,Turf+15.1f},{3011,-11434,Turf+15.1f},{3011,-11386,Turf+15.1f},{2949,-11386,Turf+15.1f},FVector::UpVector,{0,1},{1,1},{1,0},{0,0},{1,0,0});
 // Tape balls, a headset cable coil and dirty towels left near the original aluminium team benches/coolers.
 for(int32 I=0;I<16;++I){const FVector P(2350+Rand.FRandRange(-600,600),-11480+Rand.FRandRange(-90,70),Turf+3);Box(TEXT("SD_Cream"),P,{Rand.FRandRange(3,7),Rand.FRandRange(3,7),Rand.FRandRange(2,5)},FRotator(10,I*37,20),false,6500);}
 for(int32 Loop=0;Loop<6;++Loop)for(int32 I=0;I<20;++I){const float A=I*2*PI/20,B=(I+1)*2*PI/20;const FVector C(1790,-11465,Turf+1+Loop*.6f);const float R=18+Loop;Pipe(TEXT("SD_Dark"),C+FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,0),C+FVector(FMath::Cos(B)*R,FMath::Sin(B)*R,0),.55f,false,6500);}
 // Rolled canvas tarp: round profile, exposed rolled layers, sewn seam and buckled webbing.
 const FVector Tarp(-20,-12500,Turf+42);
 Pipe(TEXT("SD_Fabric"),Tarp-FVector(230,0,0),Tarp+FVector(230,0,0),35);
 for(float End:{-1.f,1.f})
 {
  // A shallow spiral on each cut end describes the layers rather than a plain cylinder cap.
  for(int32 J=0;J<240;++J){const float A=J*.105f,B=(J+1)*.105f,R0=3.f+J*.128f,R1=3.f+(J+1)*.128f;Pipe(TEXT("SD_FabricEdge"),Tarp+FVector(End*230.12f,FMath::Cos(A)*R0,FMath::Sin(A)*R0),Tarp+FVector(End*230.12f,FMath::Cos(B)*R1,FMath::Sin(B)*R1),.24f,false,10000);}
 }
 for(float X:{-210.f,170.f})
 {
  Pipe(TEXT("SD_Steel"),{X,-12538,Turf+4},{X,-12462,Turf+4},3);
  for(int32 J=0;J<32;++J){const float A=J*PI/16,B=(J+1)*PI/16;const FVector P(0,FMath::Cos(A)*35.15f,FMath::Sin(A)*35.15f),Q(0,FMath::Cos(B)*35.15f,FMath::Sin(B)*35.15f),C(X,-12500,Turf+42);Unique.Quad(TEXT("SD_Cream"),C+P-FVector(4,0,0),C+Q-FVector(4,0,0),C+Q+FVector(4,0,0),C+P+FVector(4,0,0),(P+Q).GetSafeNormal(),FHallUV::World(100));}
  SoftBox(TEXT("SD_Steel"),{X,-12500,Turf+78},{11,12,2},FRotator::ZeroRotator,false);Box(TEXT("SD_Dark"),{X,-12500,Turf+79.1f},{6,7,.2f},FRotator::ZeroRotator,false,9000);
 }
 for(float A:{.35f,3.45f}){const FVector V(0,FMath::Cos(A)*35.1f,FMath::Sin(A)*35.1f);Pipe(TEXT("SD_FabricEdge"),Tarp+V-FVector(229,0,0),Tarp+V+FVector(229,0,0),.3f,false,9000);}
 for(float X:{-210.f,170.f})for(float Y:{-12534.f,-12466.f})Pipe(TEXT("SD_Dark"),{X-2,Y,Turf+6},{X+2,Y,Turf+6},6);
 // A shared near round wheel kit keeps all four abandoned vehicles readable at walking distance.
 auto Wheel=[&](FVector C,float Radius,float Width)
 {
  Pipe(TEXT("SD_Dark"),C-FVector(0,Width*.5f,0),C+FVector(0,Width*.5f,0),Radius);
  for(float Side:{-1.f,1.f})
  {
   const FVector Hub=C+FVector(0,Side*(Width*.5f+.3f),0);Pipe(TEXT("SD_Aluminium"),Hub-FVector(0,.3f,0),Hub+FVector(0,.3f,0),Radius*.48f,false,14000);Pipe(TEXT("SD_Steel"),Hub-FVector(0,.6f,0),Hub+FVector(0,.6f,0),Radius*.14f,false,12000);
   for(int32 J=0;J<4;++J){const float A=J*PI*.5f;const FVector P=Hub+FVector(FMath::Cos(A)*Radius*.28f,Side*.65f,FMath::Sin(A)*Radius*.28f);Pipe(TEXT("SD_Steel"),P,P+FVector(0,Side*.6f,0),1.1f,false,9000);}
  }
  for(int32 J=0;J<20;++J){const float A=J*PI/10;const FVector N(FMath::Cos(A),0,FMath::Sin(A));SoftBox(TEXT("SD_TireTread"),C+N*(Radius-.2f),{4,Width*.85f,1.1f},FRotationMatrix::MakeFromXZ(FVector(0,1,0),N).Rotator(),false,6500);}
 };
 // Electric field cart: rounded enamel deck, battery pedestal, stitched seat and a shallow utility bed.
 const FVector Cart(4920,-13020,Turf);
 SoftBox(TEXT("SD_Cream"),Cart+FVector(0,0,37),{210,105,18});
 for(float Y:{-32.f,32.f})Box(TEXT("SD_Steel"),Cart+FVector(0,Y,25),{202,7,8});
 SoftBox(TEXT("SD_Steel"),Cart+FVector(-25,0,66),{48,82,38});
 SoftBox(TEXT("SD_PaddingDark"),Cart+FVector(-25,0,89),{57,98,9});SoftBox(TEXT("SD_PaddingDark"),Cart+FVector(3,0,115),{9,98,48},FRotator(0,0,0));
 for(float Y:{-38.f,0.f,38.f}){Pipe(TEXT("SD_SeatSeam"),Cart+FVector(-49,Y,93.6f),Cart+FVector(-3,Y,93.6f),.18f,false,6500);Pipe(TEXT("SD_SeatSeam"),Cart+FVector(-1.6f,Y,98),Cart+FVector(-1.6f,Y,133),.18f,false,6500);}
 for(float Y:{-39.f,39.f})Pipe(TEXT("SD_Steel"),Cart+FVector(10,Y,46),Cart+FVector(10,Y,133),1.9f);
 for(float X:{-75.f,75.f}){Pipe(TEXT("SD_Steel"),Cart+FVector(X,-66,25),Cart+FVector(X,66,25),3.5f);for(float Y:{-58.f,58.f})Wheel(Cart+FVector(X,Y,25),25,16);}
 for(float Y:{-49.f,49.f}){SoftBox(TEXT("SD_Cream"),Cart+FVector(63,Y,64),{84,5,36});Pipe(TEXT("SD_Aluminium"),Cart+FVector(23,Y,82),Cart+FVector(103,Y,82),1.3f,false,12000);}
 SoftBox(TEXT("SD_Cream"),Cart+FVector(103,0,64),{5,100,36});
 for(float X:{-108.f,110.f}){Pipe(TEXT("SD_Steel"),Cart+FVector(X,-49,27),Cart+FVector(X,49,27),3.5f);}
 SoftBox(TEXT("SD_Dark"),Cart+FVector(-76,0,46.8f),{44,86,1.4f},FRotator::ZeroRotator,false);
 for(int32 J=0;J<8;++J)Box(TEXT("SD_TireTread"),Cart+FVector(-77,-35+J*10,47.6f),{40,1.4f,.3f},FRotator::ZeroRotator,false,6500);
 const FVector Steering=Cart+FVector(-80,0,112),WheelBase=Cart+FVector(-65,0,48),WheelRight(0,1,0),WheelAxis=(Steering-WheelBase).GetSafeNormal(),WheelUp=FVector::CrossProduct(WheelAxis,WheelRight);Pipe(TEXT("SD_Steel"),WheelBase,Steering,2.8f);
 for(int32 J=0;J<24;++J){const float A=J*PI/12,B=(J+1)*PI/12;Pipe(TEXT("SD_Dark"),Steering+(WheelRight*FMath::Cos(A)+WheelUp*FMath::Sin(A))*17,Steering+(WheelRight*FMath::Cos(B)+WheelUp*FMath::Sin(B))*17,1.5f);}for(int32 J=0;J<3;++J){const float A=J*2*PI/3;Pipe(TEXT("SD_Steel"),Steering,Steering+(WheelRight*FMath::Cos(A)+WheelUp*FMath::Sin(A))*15,1.2f);}for(float Y:{-20.f,20.f})SoftBox(TEXT("SD_Dark"),Cart+FVector(-79,Y,49),{20,10,3},FRotator(0,0,-8));
 for(float Y:{-53.f,53.f})for(float X:{-84.f,84.f}){SoftBox(TEXT("SD_Steel"),Cart+FVector(X,Y,39),{14,1,4},FRotator::ZeroRotator,false);}
 Letter(TEXT("CS 04"),Cart+FVector(58,-52.1f,65),FVector(-1,0,0),FVector(0,-1,0),8,TEXT("SD_Purple"));
 // Hand-pushed chalk liner: tapered open hopper, pale powder, lower dispensing gate and cable lever.
 const FVector Liner(4200,-15000,Turf);const FVector Up=FVector::UpVector;
 FVector Lower[4],Upper[4];for(int32 J=0;J<4;++J){const float X=J==0||J==3?-1.f:1.f,Y=J<2?-1.f:1.f;Lower[J]=Liner+FVector(X*10,Y*8,30);Upper[J]=Liner+FVector(X*28,Y*23,77);}
 for(int32 J=0;J<4;++J){const int32 K=(J+1)%4;const FVector N=FVector::CrossProduct(Upper[J]-Lower[J],Lower[K]-Lower[J]).GetSafeNormal();Unique.Quad(TEXT("SD_Cream"),Lower[J],Lower[K],Upper[K],Upper[J],N,FHallUV::World(100));Unique.Quad(TEXT("SD_Cream"),Lower[J]+Up*.3f,Lower[K]+Up*.3f,Upper[K]-N*.8f,Upper[J]-N*.8f,-N,FHallUV::World(100));Collision.Quad(TEXT("SD_Cream"),Lower[J],Lower[K],Upper[K],Upper[J],N,FHallUV::World(100));Pipe(TEXT("SD_Aluminium"),Upper[J],Upper[K],1.2f);}
 SoftBox(TEXT("SD_Chalk"),Liner+FVector(0,0,66),{46,36,4},FRotator::ZeroRotator,true);Box(TEXT("SD_Steel"),Liner+FVector(0,0,25),{25,20,10});Box(TEXT("SD_Dark"),Liner+FVector(-1,0,18),{17,7,4});
 Pipe(TEXT("SD_Steel"),Liner+FVector(0,-34,20),Liner+FVector(0,34,20),3);for(float Y:{-29.f,29.f}){Wheel(Liner+FVector(0,Y,20),20,10);Pipe(TEXT("SD_Steel"),Liner+FVector(-20,Y,35),Liner+FVector(-100,Y,110),1.8f);}Pipe(TEXT("SD_Steel"),Liner+FVector(-100,-29,110),Liner+FVector(-100,29,110),1.8f);Pipe(TEXT("SD_Dark"),Liner+FVector(-100,-25,110),Liner+FVector(-100,25,110),2.4f);
 Pipe(TEXT("SD_Steel"),Liner+FVector(-96,-22,105),Liner+FVector(-86,-22,106),.8f,false,9000);Pipe(TEXT("SD_Dark"),Liner+FVector(-86,-22,106),Liner+FVector(-13,-9,28),.45f,false,9000);
 // Band risers/empty folding chairs at the east end, outside the goalpost and main playing rectangle.
 for(int32 R=0;R<3;++R)
 {
  const float Z=Turf+R*16.f;Box(TEXT("SD_Steel"),{5650,-14000+R*100.f,Z+9},{600,95,18});
  for(int32 C=0;C<6;++C){const FVector P(5400+C*80.f,-14000+R*100.f,Z+18);SoftBox(TEXT("SD_Padding"),P+FVector(0,0,42),{39,38,3});SoftBox(TEXT("SD_Padding"),P+FVector(0,16,67),{39,4,40});for(float Side:{-1.f,1.f}){Pipe(TEXT("SD_Steel"),P+FVector(Side*17,-20,0),P+FVector(Side*17,18,48),1.5f);Pipe(TEXT("SD_Steel"),P+FVector(Side*17,21,0),P+FVector(Side*17,-16,46),1.5f);Pipe(TEXT("SD_Steel"),P+FVector(Side*17,16,43),P+FVector(Side*17,16,88),1.5f);Pipe(TEXT("SD_Aluminium"),P+FVector(Side*16,0,24),P+FVector(Side*18,0,24),2.4f,false,9000);}}
 }
 // Contoured hard-shell trombone case with lid piping, hinges, two latches and a raised handle.
 const FVector Case(5230,-14100,Turf+13.6f);const FRotator CaseYaw(0,9,0);const FTransform CasePose(CaseYaw,Case);
 auto CaseAt=[&](FVector V){return CasePose.TransformPosition(V);};
 SoftBox(TEXT("SD_CaseLeather"),CaseAt({-13,0,-1.5f}),{118,28,22},CaseYaw);SoftBox(TEXT("SD_CaseLeather"),CaseAt({49,0,0}),{46,38,27},CaseYaw);
 const FVector2D Outline[]={{-72,-13},{26,-13},{31,-18},{70,-18},{72,16},{31,18},{26,13},{-72,13}};
 for(int32 J=0;J<8;++J){const auto A=Outline[J],B=Outline[(J+1)%8];Pipe(TEXT("SD_SeatSeam"),CaseAt({A.X,A.Y,3}),CaseAt({B.X,B.Y,3}),.5f,false,10000);}
 for(float X:{-42.f,44.f}){const float FaceY=X>20.f?19.f:14.f;SoftBox(TEXT("SD_Aluminium"),CaseAt({X,FaceY+.5f,1}),{9,2,8},CaseYaw,false);Pipe(TEXT("SD_Steel"),CaseAt({X-4,-FaceY-.3f,0}),CaseAt({X+4,-FaceY-.3f,0}),1.6f,false,9000);}
 Pipe(TEXT("SD_Steel"),CaseAt({-12,14.6f,0}),CaseAt({-12,22,4}),1.1f);Pipe(TEXT("SD_Dark"),CaseAt({-12,22,4}),CaseAt({12,22,4}),1.8f);Pipe(TEXT("SD_Steel"),CaseAt({12,22,4}),CaseAt({12,14.6f,0}),1.1f);
 // Abandoned parade trailer moved inward of the stair toes; no original step or route moves.
 const FVector Frame(-350,-13900,Turf+30);for(float Y:{-80.f,80.f})Pipe(TEXT("SD_Rust"),Frame+FVector(-160,Y,0),Frame+FVector(160,Y,0),4);for(float X:{-160.f,-50.f,60.f,160.f})Pipe(TEXT("SD_Rust"),Frame+FVector(X,-80,0),Frame+FVector(X,80,0),4);for(float X:{-100.f,110.f})for(float Y:{-88.f,88.f})Wheel(Frame+FVector(X,Y,-8),22,14);
 for(float Y:{-70.f,70.f})Pipe(TEXT("SD_Rust"),Frame+FVector(-160,Y,0),Frame+FVector(-275,0,-2),3);SoftBox(TEXT("SD_Steel"),Frame+FVector(-281,0,-2),{18,10,8});Pipe(TEXT("SD_Steel"),Frame+FVector(-230,0,-4),Frame+FVector(-230,0,-27),2.5f);Box(TEXT("SD_Steel"),Frame+FVector(-230,0,-28),{14,14,3});
 for(int32 J=0;J<3;++J)SoftBox(TEXT("SD_Wood"),Frame+FVector(-100+J*70,0,5),{18,160,3},FRotator(0,0,J-1));
 for(int32 I=0;I<160;++I){const FVector P=Frame+FVector(Rand.FRandRange(-250,250),Rand.FRandRange(-140,140),-29.88f);Box(I%2?TEXT("SD_Gold"):TEXT("SD_Purple"),P,{Rand.FRandRange(1,4),Rand.FRandRange(1,5),.15f},FRotator(0,I*13,0),false,7000);}
 for(int32 C=0;C<3;++C)for(int32 J=0;J<18;++J){const float A=J*.3f,B=(J+1)*.3f;const FVector Origin=Frame+FVector(C*70-80,-110,-29.5f);Pipe(C%2?TEXT("SD_Gold"):TEXT("SD_Purple"),Origin+FVector(J*10,FMath::Sin(A)*16,1),Origin+FVector((J+1)*10,FMath::Sin(B)*16,1),1.6f,false,7000);}
}
