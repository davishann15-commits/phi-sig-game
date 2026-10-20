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
