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
 const FVector Base(2780,-11455,Turf+2);
 for(int32 I=0;I<90;++I){const float T=I/89.f;const FVector P=Base+FVector(T*740,FMath::Sin(T*PI*2)*32,I%2?2.f:0.f),Side=I%2?FVector(0,0,1):FVector(0,1,0);for(int32 J=0;J<12;++J){const float A=J*PI/6,B=(J+1)*PI/6;Pipe(TEXT("SD_Aluminium"),P+FVector(FMath::Cos(A)*4.4f,0,0)+Side*(FMath::Sin(A)*2.f),P+FVector(FMath::Cos(B)*4.4f,0,0)+Side*(FMath::Sin(B)*2.f),.48f,false,6500);}}
 for(float X:{2760.f,3500.f}){Pipe(TEXT("SD_Orange"),{X,-11472,Turf+9},{X+230,-11425,Turf+9},5,true,20000);Box(TEXT("SD_Orange"),{X,-11472,Turf+10},{50,36,18},FRotator(0,12,0));}
 Box(TEXT("SD_Dark"),{2980,-11410,Turf+7.5f},{64,15,50},FRotator(0,0,90));Unique.QuadUV(TEXT("SD_DownBoard"),{2949,-11434,Turf+15.1f},{3011,-11434,Turf+15.1f},{3011,-11386,Turf+15.1f},{2949,-11386,Turf+15.1f},FVector::UpVector,{0,1},{1,1},{1,0},{0,0},{1,0,0});
 // Tape balls, a headset cable coil and dirty towels left near the original aluminium team benches/coolers.
 for(int32 I=0;I<16;++I){const FVector P(2350+Rand.FRandRange(-600,600),-11480+Rand.FRandRange(-90,70),Turf+3);Box(TEXT("SD_Cream"),P,{Rand.FRandRange(3,7),Rand.FRandRange(3,7),Rand.FRandRange(2,5)},FRotator(10,I*37,20),false,6500);}
 for(int32 Loop=0;Loop<6;++Loop)for(int32 I=0;I<20;++I){const float A=I*2*PI/20,B=(I+1)*2*PI/20;const FVector C(1790,-11465,Turf+1+Loop*.6f);const float R=18+Loop;Pipe(TEXT("SD_Dark"),C+FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,0),C+FVector(FMath::Cos(B)*R,FMath::Sin(B)*R,0),.55f,false,6500);}
 // Rolled canvas tarp, straps and a low wheeled storage cradle.
 Pipe(TEXT("SD_Fabric"),{-250,-12500,Turf+42},{210,-12500,Turf+42},35);for(float X:{-210.f,170.f}){Pipe(TEXT("SD_Steel"),{X,-12538,Turf+4},{X,-12462,Turf+4},3);for(int32 J=0;J<16;++J){const float A=(J+.5f)*PI/8;const FVector Radial(0,FMath::Cos(A),FMath::Sin(A));Box(TEXT("SD_Cream"),FVector(X,-12500,Turf+42)+Radial*35.3f,{8,13.9f,.5f},FRotationMatrix::MakeFromXZ(FVector::ForwardVector,Radial).Rotator(),false,9000);}}
 for(float X:{-210.f,170.f})for(float Y:{-12534.f,-12466.f})Pipe(TEXT("SD_Dark"),{X-2,Y,Turf+6},{X+2,Y,Turf+6},6);
 // Field-maintenance electric cart: wheels, axle, low chassis, bench, pedals/steering, open utility bed.
 const FVector Cart(4920,-13020,Turf);
 Box(TEXT("SD_Cream"),Cart+FVector(0,0,37),{210,105,18});Box(TEXT("SD_Steel"),Cart+FVector(-25,0,66),{55,95,38});Box(TEXT("SD_Dark"),Cart+FVector(-25,0,89),{57,98,7});Box(TEXT("SD_Dark"),Cart+FVector(3,0,115),{8,98,48});
 for(float X:{-75.f,75.f}){Pipe(TEXT("SD_Steel"),Cart+FVector(X,-66,25),Cart+FVector(X,66,25),3.5f);for(float Y:{-58.f,58.f}){Pipe(TEXT("SD_Dark"),Cart+FVector(X,Y-8,25),Cart+FVector(X,Y+8,25),25);Pipe(TEXT("SD_Steel"),Cart+FVector(X,Y-9,25),Cart+FVector(X,Y+9,25),9);}}
 for(float Y:{-49.f,49.f})Box(TEXT("SD_Cream"),Cart+FVector(63,Y,64),{84,5,36});Box(TEXT("SD_Cream"),Cart+FVector(103,0,64),{5,100,36});
 const FVector Wheel=Cart+FVector(-80,0,112),WheelBase=Cart+FVector(-65,0,48),WheelRight(0,1,0),WheelAxis=(Wheel-WheelBase).GetSafeNormal(),WheelUp=FVector::CrossProduct(WheelAxis,WheelRight);Pipe(TEXT("SD_Steel"),WheelBase,Wheel,2.8f);
 for(int32 I=0;I<16;++I){const float A=I*PI/8,B=(I+1)*PI/8;Pipe(TEXT("SD_Dark"),Wheel+(WheelRight*FMath::Cos(A)+WheelUp*FMath::Sin(A))*17,Wheel+(WheelRight*FMath::Cos(B)+WheelUp*FMath::Sin(B))*17,1.5f);}for(int32 I=0;I<3;++I){const float A=I*2*PI/3;Pipe(TEXT("SD_Steel"),Wheel,Wheel+(WheelRight*FMath::Cos(A)+WheelUp*FMath::Sin(A))*15,1.2f);}for(float Y:{-20.f,20.f})Box(TEXT("SD_Dark"),Cart+FVector(-79,Y,48),{20,10,3});
 // A hand-pushed chalk liner, not a modern robotic grounds device.
 const FVector Liner(4200,-15000,Turf);Box(TEXT("SD_Cream"),Liner+FVector(0,0,50),{56,42,48});Pipe(TEXT("SD_Steel"),Liner+FVector(0,-34,20),Liner+FVector(0,34,20),3);for(float Y:{-16.f,16.f})Box(TEXT("SD_Steel"),Liner+FVector(0,Y,25),{8,6,12});for(float Y:{-29.f,29.f}){Pipe(TEXT("SD_Dark"),Liner+FVector(0,Y-5,20),Liner+FVector(0,Y+5,20),20);Pipe(TEXT("SD_Steel"),Liner+FVector(-28,Y,45),Liner+FVector(-100,Y,110),1.8f);}Pipe(TEXT("SD_Steel"),Liner+FVector(-100,-29,110),Liner+FVector(-100,29,110),1.8f);
 // Band risers/empty folding chairs at the east end, outside the goalpost and main playing rectangle.
 for(int32 R=0;R<3;++R)
 {
  //18cm first step, then16cm rises. The east130cm stays free as the access aisle.
  const float Z=Turf+R*16.f;Box(TEXT("SD_Steel"),{5650,-14000+R*100.f,Z+9},{600,95,18});
  for(int32 C=0;C<6;++C){const FVector P(5400+C*80.f,-14000+R*100.f,Z+18);Box(TEXT("SD_Padding"),P+FVector(0,0,42),{39,38,3});Box(TEXT("SD_Padding"),P+FVector(0,16,67),{39,4,40});for(float S:{-1.f,1.f}){Pipe(TEXT("SD_Steel"),P+FVector(S*17,-20,0),P+FVector(S*17,18,48),1.5f);Pipe(TEXT("SD_Steel"),P+FVector(S*17,21,0),P+FVector(S*17,-16,46),1.5f);Pipe(TEXT("SD_Steel"),P+FVector(S*17,16,43),P+FVector(S*17,16,88),1.5f);}}
 }
 const FVector Case(5230,-14100,Turf+12.55f);Box(TEXT("SD_Dark"),Case,{145,34,25},FRotator(0,9,0));for(float X:{-45.f,45.f})Box(TEXT("SD_Aluminium"),Case+FVector(X,18,0),{12,2,8});Pipe(TEXT("SD_Steel"),Case+FVector(-13,19,0),Case+FVector(13,19,0),2);
 // Skeletal float frame left at the other end, separate from the original tissue-goose float on the campus green.
 const FVector Frame(-750,-14250,Turf+30);for(float Y:{-80.f,80.f})Pipe(TEXT("SD_Rust"),Frame+FVector(-160,Y,0),Frame+FVector(160,Y,0),4);for(float X:{-160.f,-50.f,60.f,160.f})Pipe(TEXT("SD_Rust"),Frame+FVector(X,-80,0),Frame+FVector(X,80,0),4);for(float X:{-100.f,110.f})for(float Y:{-88.f,88.f})Pipe(TEXT("SD_Dark"),Frame+FVector(X,Y-7,-8),Frame+FVector(X,Y+7,-8),22);
 for(int32 I=0;I<160;++I){const FVector P=Frame+FVector(Rand.FRandRange(-250,250),Rand.FRandRange(-140,140),-29.88f);Box(I%2?TEXT("SD_Gold"):TEXT("SD_Purple"),P,{Rand.FRandRange(1,4),Rand.FRandRange(1,5),.15f},FRotator(0,I*13,0),false,7000);}
 for(int32 C=0;C<3;++C)for(int32 J=0;J<18;++J){const float A=J*.3f,B=(J+1)*.3f;const FVector Origin=Frame+FVector(C*70-80,-110,-29.5f);Pipe(C%2?TEXT("SD_Gold"):TEXT("SD_Purple"),Origin+FVector(J*10,FMath::Sin(A)*16,1),Origin+FVector((J+1)*10,FMath::Sin(B)*16,1),1.6f,false,7000);}
}
