from pathlib import Path
r=Path(__file__).resolve().parents[3];f=Path(__file__).parent
def rep(t,a,b):assert t.count(a)==1,a[:120];return t.replace(a,b,1)
t=(f/'HouseStadiumDetailLook.cpp.before').read_text();start=t.index('  HallMakePlain(Look,TEXT("SD_CoachEnamel")');end=t.index('  FHallImage Roster;',start)
t=t[:start]+'''
  // Owner requested more age and wear: actual CC0 furniture scans, kept separate from existing locker wood.
  auto* FurnitureWood=LoadObject<UTexture2D>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Stadium/Textures/T_Stadium_FurnitureWood_Color.T_Stadium_FurnitureWood_Color"));
  auto* FurnitureWoodN=LoadObject<UTexture2D>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Stadium/Textures/T_Stadium_FurnitureWood_Normal.T_Stadium_FurnitureWood_Normal"));
  if(FurnitureWood&&FurnitureWoodN){HallMakeTextured(Look,TEXT("SD_FurnitureOak"),FurnitureWood,FurnitureWoodN,.85f,{.62f,.50f,.36f},.55f);HallMakeTextured(Look,TEXT("SD_FurnitureOakDark"),FurnitureWood,FurnitureWoodN,.90f,{.39f,.31f,.21f},.45f);HallMakeTextured(Look,TEXT("SD_FurnitureOakRubbed"),FurnitureWood,FurnitureWoodN,.94f,{.88f,.71f,.46f},.60f);}
  else for(FName Key:{FName(TEXT("SD_FurnitureOak")),FName(TEXT("SD_FurnitureOakDark")),FName(TEXT("SD_FurnitureOakRubbed"))})Look->Materials.Add(Key,Look->Mat(TEXT("SD_LockerOakDark")));
  auto* Leather=LoadObject<UTexture2D>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Stadium/Textures/T_Stadium_FurnitureLeather_Color.T_Stadium_FurnitureLeather_Color"));
  auto* LeatherN=LoadObject<UTexture2D>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Stadium/Textures/T_Stadium_FurnitureLeather_Normal.T_Stadium_FurnitureLeather_Normal"));
  if(Leather&&LeatherN){HallMakeTextured(Look,TEXT("SD_CoachUpholstery"),Leather,LeatherN,.81f,{.54f,.40f,.28f},.75f);HallMakeTextured(Look,TEXT("SD_LeatherWorn"),Leather,LeatherN,.96f,{.96f,.70f,.40f},.85f);}
  else {HallMakePlain(Look,TEXT("SD_CoachUpholstery"),{.12f,.05f,.025f},.88f);HallMakePlain(Look,TEXT("SD_LeatherWorn"),{.22f,.105f,.04f},.96f);}
  HallMakePlain(Look,TEXT("SD_LeatherCrack"),{.034f,.016f,.009f},.99f);
  FHallImage Enamel,EnamelN;Enamel.Init(256,256,{.055f,.078f,.051f});TArray<float> EnamelH;EnamelH.SetNum(256*256);ParallelFor(256,[&](int32 Y){for(int32 X=0;X<256;++X){const float G=HallNoise(X*.8f,Y*.8f,1942),Age=HallFbm(X*.023f,Y*.025f,3,1938),Chip=HallNoise(X*.10f,Y*.12f,1961);FLinearColor C=FLinearColor(.055f,.078f,.051f)*(.75f+Age*.6f);if(Chip>.74f&&G>.42f)C=FLinearColor(.16f,.080f,.029f)*(.65f+G*.5f);Enamel.At(X,Y)=C;EnamelH[Y*256+X]=G*.04f+(Chip>.74f?.08f:0);}});HallHeightToNormal(EnamelH,256,256,.4f,true,EnamelN);L::Texture(Look,TEXT("SD_CoachEnamel"),Enamel,.84f,.20f,&EnamelN);
  FHallImage Brass;Brass.Init(128,128,{.22f,.15f,.06f});ParallelFor(128,[&](int32 Y){for(int32 X=0;X<128;++X){const float Age=HallFbm(X*.035f,Y*.04f,3,1931);Brass.At(X,Y)=FLinearColor(.25f,.17f,.064f)*(.55f+Age*.7f);}});L::Texture(Look,TEXT("SD_CoachBrass"),Brass,.77f,.65f);
  FHallImage OldPaper;OldPaper.Init(256,256,{.45f,.37f,.23f});ParallelFor(256,[&](int32 Y){for(int32 X=0;X<256;++X){const float G=HallNoise(X*2.f,Y*2.f,1986),Stain=HallFbm(X*.022f,Y*.035f,3,1979),Edge=FMath::Min(FMath::Min(X,255-X),FMath::Min(Y,255-Y))/24.f;OldPaper.At(X,Y)=FLinearColor(.49f,.41f,.27f)*(.75f+G*.1f+Stain*.2f)*FMath::Clamp(Edge,.65f,1.f);}});L::Texture(Look,TEXT("SD_CoachPaper"),OldPaper,.99f);
''' +t[end:]
# Age the authored roster without changing the approved fictional content.
t=rep(t,'Roster.Init(512,768,{.65f,.60f,.45f});','''Roster.Init(512,768,{.46f,.39f,.26f});ParallelFor(768,[&](int32 Y){for(int32 X=0;X<512;++X){const float G=HallNoise(X*.8f,Y*.8f,1985),A=HallFbm(X*.015f,Y*.012f,3,1977);Roster.At(X,Y)=FLinearColor(.48f,.405f,.275f)*(.77f+G*.11f+A*.23f);}});''')
(f/'HouseStadiumDetailLook.cpp.candidate').write_text(t)
t=(f/'HouseStadiumDetailInteriors.cpp.before').read_text();t=t.replace('||Key==TEXT("SD_BenchWood"))','||Key==TEXT("SD_BenchWood")||Key.ToString().StartsWith(TEXT("SD_FurnitureOak")))',1)
t=rep(t,' const FName BevelKey=AgedWood?FName(TEXT("SD_WoodEdge")):Key;',' const FName BevelKey=Key.ToString().StartsWith(TEXT("SD_FurnitureOak"))?FName(TEXT("SD_FurnitureOakRubbed")):AgedWood?FName(TEXT("SD_WoodEdge")):Key;')
k=' auto P=[&](FVector V){return Centre+Rotation.RotateVector(V);};';t=rep(t,k,' if(Key==TEXT("SD_CoachUpholstery"))UV=FHallUV::Local(FTransform(Rotation,Centre),40);\n'+k)
t=rep(t,'float Height)\n{const float Pitch=Height*.82f;','float Height,FName Ink=TEXT("SD_Cream"))\n{const float Pitch=Height*.82f;');t=rep(t,'HouseCampusFallKit::Bar(M,TEXT("SD_Cream"),P(Stroke[J-1])','HouseCampusFallKit::Bar(M,Ink,P(Stroke[J-1])')
a=t.index('void HouseStadiumDetail::CoachOffice(');body=t[a:]
# Furniture-specific scanned material replacement only. Preserve accepted room walls/locker palette.
for old,new in [('SD_LockerOakDark','SD_FurnitureOakDark'),('SD_LockerOakLight','SD_FurnitureOakRubbed'),('SD_LockerOak','SD_FurnitureOak'),('SD_BenchWood','SD_FurnitureOak'),('SD_NameBrass','SD_CoachBrass')]:body=body.replace('TEXT("'+old+'")','TEXT("'+new+'")')
body=body.replace('I::Box(Detail,TEXT("SD_Cream"),P(37+J','I::Box(Detail,TEXT("SD_CoachPaper"),P(37+J')
body=body.replace('I::SoftBox(Detail,TEXT("SD_RoomBeam"),File+Y*30.7f','I::SoftBox(Detail,TEXT("SD_CoachEnamel"),File+Y*30.7f')
body=body.replace('I::Box(Detail,TEXT("SD_Cream"),File+Y*31.7f','I::Box(Detail,TEXT("SD_CoachPaper"),File+Y*31.7f')
# Remove the earlier tiny, uniform edge ticks and replace them with broad irregular hand wear.
x=body.index(' // Broken coffee ring,');y=body.index(' // A green-shaded desk lamp',x)
body=body[:x]+''' // Rubbed varnish, interrupted dark water rings and small cross-grain cuts where the desk was used.
 FRandomStream Wear(19861018);for(float Side:{-1.f,1.f})for(int32 J=0;J<25;++J){const float U=-78+J*6.1f,Width=Wear.FRandRange(.5f,2.5f);Detail.Quad(TEXT("SD_FurnitureOakRubbed"),P(U,Side*38.7f,76.42f),P(U+5.5f,Side*38.7f,76.42f),P(U+5.1f,Side*(38.7f-Width*.6f),76.42f),P(U+.4f,Side*(38.7f-Width),76.42f),FVector::UpVector,FHallUV::World(60));}
 for(FVector2D Centre:{FVector2D(-51,8),FVector2D(28,-20)}){const FVector Ring=P(Centre.X,Centre.Y,76.46f);for(int32 J=0;J<40;++J)if(J%9>1){const float A0=J*2*PI/40,A1=(J+1)*2*PI/40;Detail.Cylinder(TEXT("SD_LeatherCrack"),Ring+X*(FMath::Cos(A0)*4.4f)+Y*(FMath::Sin(A0)*4.4f),Ring+X*(FMath::Cos(A1)*4.4f)+Y*(FMath::Sin(A1)*4.4f),.11f,5,FHallUV::World(100));}}
 for(int32 J=0;J<19;++J){const float U=Wear.FRandRange(-75,75),V=Wear.FRandRange(-33,33),Span=Wear.FRandRange(1.5f,5.f);Detail.Cylinder(TEXT("SD_LeatherCrack"),P(U,V,76.44f),P(U+Span,V+.4f,76.44f),.065f,4,FHallUV::World(100));}
''' +body[y:]
# Add hand-worn front apron and knees' worn patches to the actual seat surface, plus split leather at the back.
x=body.index(' // Visitor chair sits against the inner wall,')
body=body[:x]+''' // Irregular worn leather at the seat front and lumbar contact points. These are shallow cosmetic surfaces.
 auto Patch=[&](FVector Centre,FVector Across,FVector Up,FVector N,float Width,float Height){const TArray<FVector2D> Outline={{-.50f,-.28f},{-.37f,-.49f},{-.12f,-.40f},{.14f,-.50f},{.39f,-.34f},{.50f,-.08f},{.35f,.20f},{.43f,.40f},{.12f,.50f},{-.13f,.37f},{-.37f,.48f},{-.47f,.14f}};TArray<FVector> Edge;for(FVector2D Q:Outline)Edge.Add(Centre+Across*(Q.X*Width)+Up*(Q.Y*Height));HouseCampusFallKit::Fan(Detail,TEXT("SD_LeatherWorn"),Centre,Edge,N);};
 for(float Side:{-1.f,1.f}){Patch(Chair+X*(Side*12)+Y*17+FVector(0,0,47.56f),X,Y,FVector::UpVector,15,6.5f);Patch(Chair+X*(Side*11)-Y*16.94f+FVector(0,0,72),X,FVector::UpVector,Y,12,10);}
 for(int32 J=0;J<9;++J){const float U=-18+J*4.3f;Detail.Cylinder(TEXT("SD_LeatherCrack"),Chair+X*U+Y*16+FVector(0,0,47.60f),Chair+X*(U+1.7f)+Y*19+FVector(0,0,47.60f),.10f,5,FHallUV::World(100));}
 // An old stitched repair on the seat's right side, kept small enough to feel incidental.
 const FVector Repair=Chair+X*18+Y*2+FVector(0,0,47.58f);HouseCampusFallKit::Card(Detail,TEXT("SD_CaseLeather"),Repair,X,Y,6,9,FVector::UpVector);
 for(float Side:{-1.f,1.f})for(int32 J=0;J<6;++J)Detail.Cylinder(TEXT("SD_Cream"),Repair+X*(Side*2.6f)+Y*(-3.5f+J*1.4f)+FVector(0,0,.1f),Repair+X*(Side*3.3f)+Y*(-3.3f+J*1.4f)+FVector(0,0,.1f),.09f,5,FHallUV::World(100));
''' +body[x:]
# Cabinet labels are legible and aged, instead of four blank clean pieces of paper.
x=body.index(' // Low timber bookcase with mismatched binders;')
body=body[:x]+''' const TCHAR* Files[]={TEXT("BILLS"),TEXT("1984"),TEXT("1985"),TEXT("ROSTER")};for(int32 J=0;J<4;++J)I::Word(Detail,Files[J],File+Y*32+FVector(0,0,19.8f+J*29),X,Y,2.f,TEXT("SD_LeatherCrack"));
 for(float Side:{-1.f,1.f})for(int32 J=0;J<9;++J){const float Z=8+J*13;const FVector Q=File+X*(Side*20.5f)+Y*30.85f+FVector(0,0,Z);HouseCampusFallKit::Tri(Detail,TEXT("SD_Rust"),Q-X*.5f,Q+X*.6f+FVector(0,0,1),Q+FVector(0,0,3.2f),Y,FHallUV::World(100));}
''' +body[x:]
t=t[:a]+body;(f/'HouseStadiumDetailInteriors.cpp.candidate').write_text(t)
print('Prepared owned furniture wear candidates outside runtime; import, apply, compile and native proof remain.')
