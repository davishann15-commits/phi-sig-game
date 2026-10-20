 // Owner room-identity proof: real worn plaster, quieter mineral floor and purposeful utilities.
 if(Step()>=7){
  auto* Wall=LoadObject<UTexture2D>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Stadium/Textures/T_Stadium_RoomPlaster_Color.T_Stadium_RoomPlaster_Color"));
  auto* WallN=LoadObject<UTexture2D>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Stadium/Textures/T_Stadium_RoomPlaster_Normal.T_Stadium_RoomPlaster_Normal"));
  if(Wall&&WallN){HallMakeTextured(Look,TEXT("SD_RoomPlaster"),Wall,WallN,.94f,{.83f,.79f,.66f},.52f);HallMakeTextured(Look,TEXT("SD_RoomCeiling"),Wall,WallN,.97f,{.49f,.47f,.41f},.40f);}
  else {Look->Materials.Add(TEXT("SD_RoomPlaster"),Look->Mat(TEXT("SD_Concrete")));Look->Materials.Add(TEXT("SD_RoomCeiling"),Look->Mat(TEXT("SD_Concrete")));}
  if(ScanColor&&ScanNormal)HallMakeTextured(Look,TEXT("SD_RoomFloor"),ScanColor,ScanNormal,.84f,{.36f,.35f,.33f},.45f);else Look->Materials.Add(TEXT("SD_RoomFloor"),Look->Mat(TEXT("SD_Concrete")));
  HallMakePlain(Look,TEXT("SD_RoomBeam"),{.32f,.31f,.27f},.91f,.12f);HallMakePlain(Look,TEXT("SD_RoomPipe"),{.22f,.20f,.15f},.78f,.30f);
  HallMakePlain(Look,TEXT("SD_RoomTube"),{.67f,.64f,.51f},.46f,0,{4.2f,4.0f,3.25f});HallMakePlain(Look,TEXT("SD_RoomTubeOld"),{.40f,.36f,.24f},.64f,0,{1.3f,1.1f,.65f});
  FHallImage Stripe;Stripe.Init(1024,128,{.10f,.055f,.12f});ParallelFor(128,[&](int32 Y){for(int32 X=0;X<1024;++X){const float Fine=HallNoise(X*.75f,Y*.7f,31),Patch=HallFbm(X*.015f,Y*.07f,3,1986);auto C=FLinearColor(.13f,.073f,.15f)*(.8f+Patch*.35f);if(Y>106&&Y<116)C={.43f,.32f,.12f};if(Fine>.78f||Patch>.68f)C=FLinearColor(.48f,.45f,.36f)*(.8f+Fine*.2f);Stripe.At(X,Y)=C;}});L::Texture(Look,TEXT("SD_RoomStripe"),Stripe,.94f);
  FHallImage Notes;Notes.Init(1024,768,{.16f,.091f,.042f});ParallelFor(768,[&](int32 Y){for(int32 X=0;X<1024;++X)Notes.At(X,Y)=FLinearColor(.21f,.124f,.060f)*(.7f+HallNoise(X*.7f,Y*.7f,1931)*.45f);});
  Notes.FillPolygon({{45,46},{587,35},{601,392},{53,405}},{.58f,.52f,.38f});Notes.FillPolygon({{633,74},{965,94},{950,335},{619,312}},{.64f,.59f,.43f});Notes.FillPolygon({{402,447},{951,422},{966,704},{414,718}},{.69f,.64f,.49f});Notes.FillPolygon({{65,452},{335,469},{322,706},{52,689}},{.46f,.39f,.22f});
  auto Note=[&](const TCHAR* T,float X,float Y,float H,bool Hand=false){FHallTextStyle S;S.Height=H;S.Stroke=H*(Hand?.063f:.065f);S.Spacing=.18f;S.Wobble=Hand?1.25f:.2f;S.Seed=int32(X+Y)+1986;HallDrawText(Notes,T,{X,Y},S,{.075f,.061f,.043f});};
  Note(TEXT("CALDER CHRONICLE"),77,100,39);Note(TEXT("SATURDAY OCT 18 1986"),78,141,22);Notes.Stroke({{76,155},{565,155}},2,{.12f,.10f,.06f});Note(TEXT("GEESE HOLD ON"),78,210,41);Note(TEXT("24 - 17"),115,283,61);Note(TEXT("HOMECOMING AT CALDER"),79,332,22);Note(TEXT("ONE LAST HOME GAME"),79,373,22);
  Note(TEXT("MONDAY 8 AM"),660,146,30,true);Note(TEXT("RETURN ALL"),649,200,29,true);Note(TEXT("EQUIPMENT"),645,249,30,true);Note(TEXT("- COACH"),686,292,26,true);
  Note(TEXT("WARD - CALL HOME"),444,503,32,true);Note(TEXT("MOM SAVED THE PAPER"),440,558,28,true);Note(TEXT("KEEP THE GAME BALL"),440,617,29,true);Note(TEXT("THANK YOU SENIORS"),440,672,29,true);
  Note(TEXT("1978"),95,514,39);Note(TEXT("LEAGUE"),81,569,31);Note(TEXT("CHAMPS"),74,618,32);Note(TEXT("CALDER STATE"),70,662,19);L::Texture(Look,TEXT("SD_RoomNotices"),Notes,.97f,0,nullptr,false);
 }
