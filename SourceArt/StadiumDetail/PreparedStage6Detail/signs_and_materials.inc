namespace HouseStadiumDetailSignImpl
{
void Line(FHallImage& I,const FString& Text,float Y,float H,FLinearColor Ink,float Width=.90f)
{
 FHallTextStyle S;S.Height=H;S.Stroke=H*.085f;S.Spacing=.16f;S.Wobble=.38f;S.Seed=19861018;
 const FString Painted=Text.Replace(TEXT("\u00a2"),TEXT("C"));S.WidthScale=FMath::Min(1.f,(I.W*Width)/FMath::Max(1.f,HallTextWidth(Painted,S)));
 const float Left=(I.W-HallTextWidth(Painted,S))*.5f;HallDrawText(I,Painted,{Left,Y},S,Ink);
 for(int32 J=0;J<Text.Len();++J)if(Text[J]==TCHAR(0xa2)){const float X=Left+(J?HallTextWidth(Painted.Left(J),S)+S.Spacing*H:0.f)+H*S.WidthScale/3.f;I.Stroke({{X+H*.07f,Y+H*.08f},{X-H*.07f,Y-H*1.08f}},S.Stroke*.8f,Ink);}
}
void Board(UHouseBlackHallSubsystem* Look,const TCHAR* Key,const TCHAR* A,const TCHAR* B,const TCHAR* C,int32 W,int32 H,FLinearColor Base,FLinearColor Ink,int32 Seed,bool Brass=false)
{
 FHallImage I;I.Init(W,H,Base);const float Border=H*.035f;
 auto Row=[&](int32 Y){for(int32 X=0;X<W;++X){const float Edge=FMath::Min(FMath::Min(float(X),float(W-1-X)),FMath::Min(float(Y),float(H-1-Y))),Grain=HallNoise(X*.8f,Y*.8f,Seed);I.At(X,Y)=Base*(.96f+Grain*.065f)*(1.f-.10f*FMath::Clamp(1.f-Edge/(H*.10f),0.f,1.f));}};ParallelFor(H,Row);
 I.Stroke({{Border,Border},{W-Border,Border},{W-Border,H-Border},{Border,H-Border},{Border,Border}},H*.010f,Ink,.82f);
 I.Stroke({{Border*1.8f,Border*1.8f},{W-Border*1.8f,Border*1.8f},{W-Border*1.8f,H-Border*1.8f},{Border*1.8f,H-Border*1.8f},{Border*1.8f,Border*1.8f}},H*.004f,Ink,.60f);
 const bool Second=FCString::Strlen(B)>0,Third=FCString::Strlen(C)>0;
 Line(I,A,H*(Second?.34f:.66f),H*(Second?.20f:.38f),Ink);
 if(Second){I.Stroke({{W*.12f,H*.43f},{W*.88f,H*.43f}},H*.007f,Ink,.58f);Line(I,B,H*(Third?.68f:.79f),H*(Third?.19f:.24f),Ink);}
 if(Third)Line(I,C,H*.88f,H*.085f,Ink);
 // A few chips at the rolled edge and screw recesses; no heavy noise over the lettering.
 FRandomStream R(Seed);for(int32 J=0;J<28;++J){const float X=R.FRandRange(3,W-3),Y=J%2?R.FRandRange(2,Border):R.FRandRange(H-Border,H-2);I.Ellipse({X,Y},{R.FRandRange(1,3),R.FRandRange(.6f,1.6f)},0,Brass?FLinearColor(.12f,.10f,.065f):FLinearColor(.26f,.25f,.22f),.70f);}
 for(float X:{Border*2.8f,W-Border*2.8f})for(float Y:{Border*2.8f,H-Border*2.8f}){I.Ellipse({X+.8f,Y+1},{H*.012f,H*.012f},0,{.08f,.073f,.06f});I.Ellipse({X,Y},{H*.009f,H*.009f},0,{.46f,.45f,.39f});I.Stroke({{X-H*.006f,Y-H*.002f},{X+H*.006f,Y+H*.002f}},H*.003f,{.07f,.067f,.05f});}
 HouseStadiumDetailLookImpl::Texture(Look,Key,I,Brass?.53f:.68f,Brass?.68f:.08f,nullptr,false);
}
}
void HouseStadiumDetail::SignLook(UHouseBlackHallSubsystem* Look,bool Polish)
{
 if(!Enabled()||Step()<6||!Look)return;namespace S=HouseStadiumDetailSignImpl;
 const FLinearColor Purple(.11f,.045f,.15f),Cream(.70f,.66f,.54f),Gold(.62f,.43f,.13f),Dark(.025f,.032f,.028f);
 struct FSpec{const TCHAR* Key;const TCHAR* A;const TCHAR* B;const TCHAR* C;int32 W,H;int32 Style;};
 const FSpec Owned[]={
 {TEXT("SD_Menu"),TEXT("GEESE GRILL"),TEXT("HOT DOG 75\u00a2"),TEXT("SODA 50\u00a2   COFFEE 35\u00a2"),1024,256,1},
 {TEXT("SD_SponsorHardware"),TEXT("ROOK & SONS"),TEXT("HARDWARE"),TEXT("SERVING CALDER SINCE 1924"),1024,384,1},
 {TEXT("SD_SponsorDiner"),TEXT("MILL CREEK DINER"),TEXT("OPEN LATE"),TEXT("PIE  COFFEE  PLATE LUNCH"),1024,384,2},
 {TEXT("SD_SponsorBank"),TEXT("RIDGE COUNTY BANK"),TEXT("YOUR HOMETOWN BANK"),TEXT("SINCE 1908"),1024,384,0},
 {TEXT("SD_Dedication"),TEXT("CALDER STATE STADIUM"),TEXT("ERECTED 1931"),TEXT("BY THE PEOPLE OF CALDER"),1024,512,3},
 {TEXT("SD_HallOfFame"),TEXT("FIGHTING GEESE"),TEXT("HALL OF FAME"),TEXT("1931  1948  1964  1978"),1024,256,0},
 {TEXT("SD_Beat"),TEXT("BEAT DUNMORE"),TEXT("CALDER STATE"),TEXT("FIGHTING GEESE"),1024,256,1},
 {TEXT("CF_TicketSign"),TEXT("TICKETS"),TEXT("SOLD OUT"),TEXT("HOMECOMING  OCT 18 1986"),1024,256,1},
 {TEXT("CF_PressSign"),TEXT("CALDER STATE STADIUM"),TEXT(""),TEXT(""),2048,256,0},
 {TEXT("CI_StaMenuGrill"),TEXT("GEESE GRILL"),TEXT("HOT DOGS 75\u00a2   BURGERS $1.25"),TEXT("COFFEE 35\u00a2  -  THANK YOU CALDER"),1536,384,1},
 {TEXT("CI_StaMenuPopcorn"),TEXT("POPCORN AND CANDY"),TEXT("POPCORN 50\u00a2   CANDY 40\u00a2"),TEXT("PEANUTS 50\u00a2  -  GO GEESE"),1536,384,0},
 {TEXT("CI_StaMenuDrinks"),TEXT("COLD DRINKS"),TEXT("COLA 60\u00a2   ORANGE 60\u00a2"),TEXT("HOT COCOA 50\u00a2"),1536,384,2},
 {TEXT("CI_StaClosed"),TEXT("SORRY"),TEXT("CLOSED"),TEXT("THANK YOU - SEE YOU NEXT GAME"),512,256,1},
 {TEXT("CI_StaGate4"),TEXT("GATE 4"),TEXT("CALDER STATE STADIUM"),TEXT(""),1024,256,0},
 {TEXT("CI_StaGate5"),TEXT("GATE 5"),TEXT("CALDER STATE STADIUM"),TEXT(""),1024,256,0},
 {TEXT("CI_StaSections"),TEXT("SECTIONS 21 - 24"),TEXT("BOWL ENTRANCE"),TEXT(""),1024,256,0},
 {TEXT("CI_StaMen"),TEXT("MEN"),TEXT("RESTROOM"),TEXT(""),512,192,2},
 {TEXT("CI_StaWomen"),TEXT("WOMEN"),TEXT("RESTROOM"),TEXT(""),512,192,2},
 {TEXT("CI_StaLockerSign"),TEXT("FIGHTING GEESE"),TEXT("PLAYERS ONLY"),TEXT("LOCKER ROOM"),768,256,0},
 {TEXT("CI_StaTunnelSign"),TEXT("PLAY LIKE A GOOSE TODAY"),TEXT("CALDER STATE FOOTBALL"),TEXT(""),1536,256,0},
 {TEXT("CI_StaGameBall"),TEXT("GAME BALL"),TEXT("CALDER 24 - DUNMORE 17"),TEXT("HOMECOMING  OCT 18 1986"),512,512,1},
 {TEXT("CI_StaRadio"),TEXT("GEESE RADIO"),TEXT("NETWORK"),TEXT("CALDER STATE  -  PRESS CREDENTIALS ONLY"),768,256,0},
 {TEXT("CI_StaScoreSign"),TEXT("PA - SCOREBOARD"),TEXT("AUTHORIZED STAFF ONLY"),TEXT(""),768,192,2},
 {TEXT("SD_TrainerLabel"),TEXT("TRAINING"),TEXT("SUPPLIES"),TEXT("CALDER STATE"),256,256,1},
 {TEXT("SD_AgitatorLabel"),TEXT("CALDER EQUIPMENT"),TEXT("MODEL W-31"),TEXT(""),512,128,1}};
 const FSpec Exterior[]={
 {TEXT("CP_SouvenirSign"),TEXT("SOUVENIRS"),TEXT("PENNANTS $2   BUTTONS 50\u00a2"),TEXT("FIGHTING GEESE  -  HOMECOMING 86"),1024,384,1},
 {TEXT("CP_StandSignA"),TEXT("HOT DOGS - POPCORN - POP"),TEXT("GEESE GRILL"),TEXT(""),1536,256,0},
 {TEXT("CP_StandSignB"),TEXT("HOT CIDER & DONUTS"),TEXT("CIDER MILL"),TEXT(""),1536,256,1},
 {TEXT("CP_ToiletSign"),TEXT("UNIT 14"),TEXT("SANITIZED"),TEXT("CALDER SERVICES"),512,256,1},
 {TEXT("CP_BandSign"),TEXT("MARCHING GEESE"),TEXT("BAND - DO NOT LEAVE INSTRUMENTS"),TEXT("CALDER STATE"),1024,256,1},
 {TEXT("CP_FriezeName"),TEXT("CALDER STATE STADIUM"),TEXT(""),TEXT(""),2048,256,1},
 {TEXT("CP_FriezeNameShort"),TEXT("CALDER STATE STADIUM"),TEXT(""),TEXT(""),1536,256,1},
 {TEXT("CP_FriezeErected"),TEXT("ERECTED BY THE ALUMNI"),TEXT(""),TEXT(""),2048,256,1},
 {TEXT("CP_FriezeDedicated"),TEXT("DEDICATED OCTOBER 17 1931"),TEXT(""),TEXT(""),2048,256,1}};
 auto Paint=[&](const FSpec& B,int32 Index){const FLinearColor Base=B.Style==0?Purple:B.Style==2?Dark:B.Style==3?FLinearColor(.32f,.235f,.11f):Cream,Ink=B.Style==0?Gold:B.Style==2?Cream:B.Style==3?FLinearColor(.055f,.044f,.025f):Purple;S::Board(Look,B.Key,B.A,B.B,B.C,B.W,B.H,Base,Ink,1931+Index,B.Style==3);};
 if(Polish){
  for(int32 I=0;I<UE_ARRAY_COUNT(Exterior);++I)Paint(Exterior[I],I+100);
  const TCHAR* Items[][6]={{TEXT("HOT DOGS"),TEXT("POPCORN"),TEXT("POP"),TEXT("COFFEE"),TEXT("CANDY"),TEXT("PEANUTS")},{TEXT("HOT CIDER"),TEXT("DONUTS"),TEXT("COFFEE"),TEXT("COCOA"),TEXT("CARAMEL APPLE"),TEXT("")}};
  const TCHAR* Prices[][6]={{TEXT("75\u00a2"),TEXT("50\u00a2"),TEXT("50\u00a2"),TEXT("35\u00a2"),TEXT("25\u00a2"),TEXT("40\u00a2")},{TEXT("50\u00a2"),TEXT("3/$1"),TEXT("35\u00a2"),TEXT("45\u00a2"),TEXT("60\u00a2"),TEXT("")}};
  for(int32 Menu=0;Menu<2;++Menu){FHallImage I;I.Init(768,960,Cream);
   ParallelFor(960,[&](int32 Y){for(int32 X=0;X<768;++X){I.At(X,Y)=Cream*(.94f+HallNoise(X*.4f,Y*.4f,Menu+1986)*.07f);}});
   I.Stroke({{16,16},{752,16},{752,944},{16,944},{16,16}},7,Purple);S::Line(I,Menu==0?TEXT("GEESE GRILL"):TEXT("CIDER MILL"),114,65,Purple);
   I.Stroke({{48,146},{720,146}},3,Purple);for(int32 Row=0;Row<(Menu==0?6:5);++Row){const float Y=250+Row*103.f;FHallTextStyle T;T.Height=38;T.Stroke=3.3f;T.Spacing=.12f;T.Wobble=.4f;T.WidthScale=.85f;HallDrawText(I,Items[Menu][Row],{52,Y},T,Dark);const FString Price=Prices[Menu][Row];FHallImage Small;Small.Init(176,76,Cream);S::Line(Small,Price,56,37,{.34f,.068f,.043f});for(int32 Py=0;Py<76;++Py)for(int32 Px=0;Px<176;++Px)I.At(542+Px,int32(Y)-56+Py)=Small.At(Px,Py);I.Stroke({{52,Y+22},{716,Y+22}},1,Gold,.5f);}
   S::Line(I,TEXT("CASH ONLY  -  THANK YOU CALDER"),900,25,Purple);HouseStadiumDetailLookImpl::Texture(Look,Menu==0?TEXT("CP_PriceBoardA"):TEXT("CP_PriceBoardB"),I,.83f,0,nullptr,false);
  }
 }else{for(int32 I=0;I<UE_ARRAY_COUNT(Owned);++I)Paint(Owned[I],I);}
 UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_SIGNS group=%s detailedBoards=%d dated=1986 freeRuntimeArt=1 newLights=0"),Polish?TEXT("exterior"):TEXT("interior_and_scoreboard"),Polish?int32(UE_ARRAY_COUNT(Exterior)):int32(UE_ARRAY_COUNT(Owned)));
}
void HouseStadiumDetail::SignHardware(FHallMesh& M,FVector C,FVector Right,FVector Normal,float W,float H)
{
 if(!Enabled()||Step()<6)return;const FRotator R=Right.Rotation();
 auto Box=[&](FVector P,FVector D,FName K){M.Box(K,FTransform(R,P),D*.5f,FHallUV::World(100));};
 for(float S:{-1.f,1.f}){Box(C+Right*(S*(W*.5f-.65f))+Normal*.18f,{1.3f,.6f,H},TEXT("SD_Steel"));Box(C+FVector(0,0,S*(H*.5f-.65f))+Normal*.18f,{W,.6f,1.3f},TEXT("SD_Steel"));}
 for(float X:{-1.f,1.f})for(float Z:{-1.f,1.f}){const FVector P=C+Right*(X*(W*.5f-2.1f))+FVector(0,0,Z*(H*.5f-2.1f))+Normal*.55f;M.Cylinder(TEXT("SD_Aluminium"),P,P+Normal*.3f,.48f,8,FHallUV::World(100));const FVector A=P+Normal*.32f-Right*.28f,B=P+Normal*.32f+Right*.28f;M.Quad(TEXT("SD_Dark"),A-FVector(0,0,.06f),B-FVector(0,0,.06f),B+FVector(0,0,.06f),A+FVector(0,0,.06f),Normal,FHallUV::World(100));}
}
