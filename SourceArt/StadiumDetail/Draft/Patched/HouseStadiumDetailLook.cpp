#include "HouseStadiumDetail.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallPaint.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
namespace HouseStadiumDetailLookImpl
{
void Texture(UHouseBlackHallSubsystem* Look,FName Key,const FHallImage& Image,float Rough,float Metal=0.f,const FHallImage* Normal=nullptr,bool Wrap=true)
{auto* T=HallMakeTexture(Image,true,false,*FString::Printf(TEXT("CalderStadium_%s"),*Key.ToString()),Wrap);Look->Textures.Add(Key,T);UTexture2D* N=nullptr;if(Normal){N=HallMakeTexture(*Normal,false,true,*FString::Printf(TEXT("CalderStadium_%s_N"),*Key.ToString()),Wrap);Look->Textures.Add(FName(*FString::Printf(TEXT("%s_N"),*Key.ToString())),N);}HallMakeTextured(Look,Key,T,N,Rough,FLinearColor::White,Normal?.65f:0.f,Metal);}
void Label(FHallImage& I,FString Text,float Y,float H,FLinearColor Ink)
{FHallTextStyle S;S.Height=H;S.Stroke=H*.075f;S.Wobble=.5f;S.Seed=861018;const FString Painted=Text.Replace(TEXT("\u00a2"),TEXT("C"));const float Left=(I.W-HallTextWidth(Painted,S))*.5f;HallDrawText(I,Painted,{Left,Y},S,Ink);for(int32 J=0;J<Text.Len();++J)if(Text[J]==TCHAR(0xa2)){const float X=Left+(J?HallTextWidth(Painted.Left(J),S)+S.Spacing*H:0.f)+H/3.f;I.Stroke({{X+H*.07f,Y+H*.08f},{X-H*.07f,Y-H*1.08f}},S.Stroke*.8f,Ink);}}
}
void HouseStadiumDetail::WeatherField(FHallImage& Image)
{
 if(!Enabled()||Step()<4)return;
 for(int32 Y=0;Y<Image.H;++Y)for(int32 X=0;X<Image.W;++X)
 {
  auto& Pixel=Image.At(X,Y);const float Grain=HallNoise(X*1.7f,Y*.9f,861018);
  // Chalk wears into the grass in small chips. Retain the original painted layout and legibility.
  if(Pixel.R>.55f&&Pixel.G>.55f&&Pixel.B>.5f&&Grain<.30f)
   Pixel=FMath::Lerp(Pixel,FLinearColor(.05f,.08f,.035f),FMath::Clamp((.30f-Grain)*3.f,0.f,.5f));
  Pixel=Pixel*(.88f+Grain*.22f);Pixel.A=1.f;
 }
}
void HouseStadiumDetail::CreateLook(UHouseBlackHallSubsystem* Look)
{
 if(!Enabled()||!Look||Look->Materials.Contains(TEXT("SD_Concrete")))return;namespace L=HouseStadiumDetailLookImpl;
 struct FSurface{const TCHAR* Key;FLinearColor Base;float Rough,Metal;int Kind;};
 const FSurface Surfaces[]={
 {TEXT("SD_Concrete"),{.37f,.355f,.32f},.96f,0,0},{TEXT("SD_Aluminium"),{.48f,.49f,.48f},.42f,.85f,1},
 {TEXT("SD_Steel"),{.19f,.205f,.20f},.67f,.65f,2},{TEXT("SD_Purple"),{.105f,.042f,.16f},.76f,.3f,3},
 {TEXT("SD_Wood"),{.245f,.15f,.08f},.88f,0,4},{TEXT("SD_SeatWood"),{.245f,.15f,.08f},.88f,0,5},{TEXT("SD_Gold"),{.59f,.38f,.064f},.75f,.2f,3},
 {TEXT("SD_Rust"),{.235f,.092f,.032f},.98f,.1f,2},{TEXT("SD_Cream"),{.67f,.64f,.51f},.85f,0,3},
 {TEXT("SD_Fabric"),{.13f,.065f,.17f},.96f,0,6}};
 for(const auto& S:Surfaces){FHallImage I,N;I.Init(256,256,S.Base);TArray<float> Height;Height.SetNum(256*256);
  for(int32 Y=0;Y<256;++Y)for(int32 X=0;X<256;++X){const float Grain=HallNoise(X*.7f,Y*.7f,201986);const float Cloud=HallFbm(X*.022f,Y*.022f,3,861018);const float Streak=HallNoise(X*.15f,Y*.003f,1931);float Scale=.82f+Cloud*.27f+(Grain-.5f)*.15f;
   if(S.Kind==1)Scale=.88f+HallNoise(X*.025f,Y*2.f,31)*.16f+(Cloud-.5f)*.17f;
   if(S.Kind==4||S.Kind==5)Scale=.65f+HallNoise(X*.04f,Y*.8f,49)*.42f+FMath::Sin(Y*.8f+HallNoise(X*.1f,Y*.01f,67)*8.f)*.05f;
   if(S.Kind==6)Scale=.86f+Grain*.12f+((X+Y)%2)*.04f;
   FLinearColor C=S.Base*Scale;if(S.Kind==0){C=C*(1.f-FMath::Pow(Streak,8.f)*.35f);if(Grain>.94f)C*=.48f;}
   if((S.Kind==2||S.Kind==3)&&Cloud>.62f&&Grain>.48f)C=FMath::Lerp(C,FLinearColor(.27f,.115f,.042f),FMath::Clamp((Cloud-.62f)*4.f,0.f,.8f));
   if(S.Kind==3&&Grain>.91f)C=FLinearColor(.38f,.39f,.36f);if(S.Kind==5&&Cloud<.62f)C=FMath::Lerp(C,FLinearColor(.13f,.068f,.18f)*Scale,.78f);C.A=1.f;I.At(X,Y)=C;Height[Y*256+X]=Grain*.15f+Cloud*.2f;
  }HallHeightToNormal(Height,256,256,.8f,true,N);L::Texture(Look,S.Key,I,S.Rough,S.Metal,&N);
 }
 // Optional owned CC0 scan, imported once by the guarded texture batch. NoStadiumDetail never loads it.
 auto* ScanColor=LoadObject<UTexture2D>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Stadium/Textures/T_Stadium_Concrete_Color.T_Stadium_Concrete_Color"));auto* ScanNormal=LoadObject<UTexture2D>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Stadium/Textures/T_Stadium_Concrete_Normal.T_Stadium_Concrete_Normal"));
 if(ScanColor&&ScanNormal){HallMakeTextured(Look,TEXT("SD_Concrete"),ScanColor,ScanNormal,.94f,{1.12f,1.12f,1.10f},.65f);UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_SURFACE concrete=PolyHaven_CC0_2K scaleCm=300 newLights=0"));}
 // Keep the original tread key, so CampusWoodsReview still counts the same geometry.
 Look->Materials.Add(TEXT("EX_CampusSeats"),Look->Mat(TEXT("SD_Concrete")));
 HallMakePlain(Look,TEXT("SD_Dark"),{.014f,.016f,.015f},.92f);HallMakePlain(Look,TEXT("SD_Chalk"),{.75f,.73f,.63f},.95f);HallMakePlain(Look,TEXT("SD_Orange"),{.64f,.17f,.021f},.76f);
 HallMakePlain(Look,TEXT("SD_Bulb"),{.58f,.34f,.10f},.32f,0,{16,8.5f,2.7f});HallMakePlain(Look,TEXT("SD_DeadBulb"),{.055f,.043f,.025f},.35f);
 HallMakePlain(Look,TEXT("SD_FlickerBulb"),{.58f,.34f,.1f},.32f,0,{14,7,2});HallMakePlain(Look,TEXT("SD_FloodFlicker"),{.6f,.63f,.68f},.2f,0,{22,24,28});
 HallMakePlain(Look,TEXT("SD_Gum"),{.08f,.073f,.064f},.98f);HallMakePlain(Look,TEXT("SD_GrassWorn"),{.092f,.10f,.043f},.98f);HallMakePlain(Look,TEXT("SD_Dirt"),{.085f,.055f,.03f},.98f);
 HallMakePlain(Look,TEXT("SD_WaterStain"),{.20f,.19f,.17f},.98f);
 HallMakePlain(Look,TEXT("SD_GrassNear"),{.035f,.075f,.023f},.98f);
 if(auto* Parent=HallHouseMat(TEXT("M_House_Glass"))){auto* Glass=UMaterialInstanceDynamic::Create(Parent,Look);Glass->SetVectorParameterValue(TEXT("Tint"),{.09f,.13f,.15f});Glass->SetScalarParameterValue(TEXT("Opacity"),.32f);Glass->SetScalarParameterValue(TEXT("Roughness"),.22f);Look->Materials.Add(TEXT("SD_Glass"),Glass);}else HallMakePlain(Look,TEXT("SD_Glass"),{.028f,.045f,.046f},.19f,.25f);
 HallMakePlain(Look,TEXT("SD_Wet"),{.035f,.038f,.031f},.14f,.1f);
 HallMakePlain(Look,TEXT("SD_Padding"),{.105f,.042f,.16f},.78f);
 struct FBoard{const TCHAR* Key;const TCHAR* A;const TCHAR* B;const TCHAR* C;};
 const FBoard Boards[]={
 {TEXT("SD_Menu"),TEXT("GEESE GRILL"),TEXT("HOT DOG 75\u00a2"),TEXT("SODA 50\u00a2  COFFEE 35\u00a2")},
 {TEXT("SD_SponsorHardware"),TEXT("ROOK & SONS"),TEXT("HARDWARE"),TEXT("SERVING CALDER SINCE 1924")},
 {TEXT("SD_SponsorDiner"),TEXT("MILL CREEK DINER"),TEXT("OPEN LATE"),TEXT("PIE  COFFEE  PLATE LUNCH")},
 {TEXT("SD_SponsorBank"),TEXT("RIDGE COUNTY BANK"),TEXT("YOUR HOMETOWN BANK"),TEXT("SINCE 1908")},
 {TEXT("SD_Dedication"),TEXT("CALDER STATE STADIUM"),TEXT("ERECTED 1931"),TEXT("BY THE PEOPLE OF CALDER")},
 {TEXT("SD_HallOfFame"),TEXT("FIGHTING GEESE"),TEXT("HALL OF FAME"),TEXT("1931  1948  1964  1978")},
 {TEXT("SD_Flyers"),TEXT("HOMECOMING OCT 18 1986"),TEXT("THE PAPER LANTERNS"),TEXT("FALL FORMAL  LOST CAT  SKI CLUB")},
 {TEXT("SD_Beat"),TEXT("BEAT DUNMORE"),TEXT("CALDER STATE"),TEXT("FIGHTING GEESE")}};
 for(const auto& B:Boards){FHallImage I;I.Init(1024,256,{.40f,.37f,.27f});L::Label(I,B.A,65,44,{.09f,.045f,.12f});L::Label(I,B.B,151,50,{.09f,.045f,.12f});L::Label(I,B.C,225,26,{.12f,.10f,.08f});L::Texture(Look,B.Key,I,.88f,0,nullptr,false);}
 FHallImage Goose;Goose.Init(256,256,{.11f,.04f,.18f});Goose.Ellipse({128,128},{118,118},0,{.72f,.49f,.08f});Goose.Ellipse({128,128},{105,105},0,{.11f,.04f,.18f});Goose.Stroke({{90,210},{105,140},{135,84}},28,{.04f,.035f,.03f});Goose.Ellipse({151,73},{34,24},-15,{.04f,.035f,.03f});Goose.Ellipse({143,85},{18,9},20,{.8f,.79f,.73f});Goose.FillPolygon({{177,64},{211,75},{177,83}},{.04f,.035f,.03f});Goose.Ellipse({160,66},{3,3},0,{.72f,.49f,.08f});L::Texture(Look,TEXT("SD_Goose"),Goose,.85f,0,nullptr,false);
 FHallImage Play;Play.Init(512,256,{.035f,.075f,.049f});for(int32 I=0;I<7;++I){const FVector2D C(80+I*52,168);Play.Ellipse(C,{9,9},0,{.58f,.65f,.57f});Play.Stroke({C+FVector2D(0,-12),C+FVector2D(0,-70),C+FVector2D(I%2?34:-34,-104)},2.8f,{.58f,.65f,.57f});}L::Label(Play,TEXT("SWEEP RIGHT"),235,27,{.58f,.65f,.57f});L::Texture(Look,TEXT("SD_Play"),Play,.98f,0,nullptr,false);
 FHallImage Down;Down.Init(256,256,{.72f,.19f,.025f});L::Label(Down,TEXT("2"),222,182,{.025f,.025f,.02f});L::Texture(Look,TEXT("SD_DownBoard"),Down,.85f,0,nullptr,false);
 if(Step()>=4){Look->Materials.Add(TEXT("CF_Cooler"),Look->Mat(TEXT("SD_Orange")));Look->Materials.Add(TEXT("CF_Bench"),Look->Mat(TEXT("SD_Aluminium")));Look->Materials.Add(TEXT("CF_Aluminium"),Look->Mat(TEXT("SD_Aluminium")));}
}
