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
 HouseCampusFallKit::Card(Clutter,TEXT("SD_AgitatorLabel"),Motor+TY*13.8f+FVector(0,0,119),-TX,FVector::UpVector,18,6,TY);
 I::Box(Clutter,TEXT("SD_Dark"),Motor+TY*14.2f+TX*7+FVector(0,0,108),{2.4f,1.6f,6},TR);
 for(float S:{-1.f,1.f})for(float Z:{89.f,116.f})I::Screw(Clutter,Motor+TY*13.6f+TX*(S*9)+FVector(0,0,Z),TY,.45f);
 const FVector Cord=Motor-TX*9-TY*6+FVector(0,0,90);Clutter.Cylinder(TEXT("SD_Dark"),Cord,Cord-TX*7+FVector(0,0,-25),.55f,8,MetalUV);
 for(int32 J=0;J<72;++J){auto P=[&](float T){return Cord-TX*7+FVector(0,0,-25)+TY*(5*FMath::Cos(T))+TX*(5*FMath::Sin(T))+FVector(0,0,-T*.40f);};const float A=J*6*PI/72;Clutter.Cylinder(TEXT("SD_Dark"),P(A),P(A+6*PI/72),.55f,8,MetalUV);}
 // Drain and graduated inner fill marks are visible through the shallow water.
 Clutter.Cylinder(TEXT("SD_Dark"),Tub+TX*25+FVector(0,0,8.1f),Tub+TX*25+FVector(0,0,8.3f),3,16,MetalUV);
 for(float Z:{30.f,40.f,50.f,60.f})I::Box(Clutter,TEXT("SD_Dark"),Tub+TY*31.8f+FVector(0,0,Z),{6,.15f,.4f},TR);
