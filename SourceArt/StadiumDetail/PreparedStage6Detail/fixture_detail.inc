 // Trainer's joinery, lower shelf, corner plates and upholstery piping; the footprint and78cm top stay fixed.
 for(float Side:{-1.f,1.f}){
  I::SoftBox(Solid,TEXT("SD_LockerOak"),Table+Y*(Side*25)+FVector(0,0,57),{159,4,13},Rot,.4f);
  I::SoftBox(Solid,TEXT("SD_LockerOakLight"),Table+X*(Side*76)+FVector(0,0,24),{6,49,5},Rot,.4f);
  for(float End:{-1.f,1.f})I::Screw(Clutter,Table+X*(End*76)+Y*(Side*27.2f)+FVector(0,0,58),Y*Side,.65f);
 }
 for(int32 J=0;J<5;++J)I::SoftBox(Solid,TEXT("SD_LockerOak"),Table+Y*((J-2)*9.5f)+FVector(0,0,24),{152,9,2},Rot,.25f);
 I::Piping(Clutter,TEXT("SD_FabricEdge"),Table,X,Y,90.6f,31.5f,73.5f,.38f);
 for(float End:{-1.f,1.f})for(int32 J=0;J<38;++J){const FVector A=Table+X*(-88+J*4.65f)+Y*(End*32.7f)+FVector(0,0,74.3f);HouseCampusFallKit::Bar(Clutter,TEXT("SD_PaddingDark"),A,A+X*1.7f,.12f,.10f,Y*End);}
 // Tape tails, scissors and a labelled amber bottle break the previous identical row of props.
 const FVector Tape=Table-X*35+FVector(0,0,78.1f);HouseCampusFallKit::Card(Clutter,TEXT("SD_Cream"),Tape+Y*9,X,Y,5,18,FVector::UpVector);
 const FVector Bottle=Table+X*47+Y*16+FVector(0,0,78);Clutter.Cylinder(TEXT("SD_BottleAmber"),Bottle,Bottle+FVector(0,0,16),3.4f,16,FHallUV::World(100));Clutter.Cylinder(TEXT("SD_Cream"),Bottle+FVector(0,0,16),Bottle+FVector(0,0,19),2.4f,16,FHallUV::World(100));HouseCampusFallKit::Card(Clutter,TEXT("SD_TrainerLabel"),Bottle-Y*3.45f+FVector(0,0,9),X,FVector::UpVector,5.7f,7,-Y);
 I::SoftBox(Clutter,TEXT("SD_Towel"),Table+X*8+Y*8+FVector(0,0,29),{43,30,7},Rot,.8f);
 for(float S:{-1.f,1.f})Clutter.Cylinder(TEXT("SD_Aluminium"),Table+X*10+Y*(S*2)+FVector(0,0,78.2f),Table+X*26+Y*(-S*2)+FVector(0,0,78.2f),.35f,8,FHallUV::World(100));
