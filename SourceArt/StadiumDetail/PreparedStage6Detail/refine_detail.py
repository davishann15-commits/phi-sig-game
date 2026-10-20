from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT/'SourceArt/StadiumDetail'));from stage_runner import idle
idle();src=ROOT/'Source/SeniorSendoff';draft=ROOT/'SourceArt/StadiumDetail/Draft'
def save(p,s):p.unlink();p.write_text(s)
def put(name,s):
 save(src/name,s)
 if name.startswith('HouseStadiumDetail'):save(draft/name,s)
p=src/'HouseCampusInteriorsStadium.cpp';s=p.read_text();s=s.replace('const float Front=D1>0?D0:D1;','const float Front=D1>0?D1:D0;');put(p.name,s)
p=src/'HouseStadiumDetailInteriors.cpp';s=p.read_text();s=s.replace('C+Along*Q.X+FVector(0,0,Q.Y)+N*','C+Along*(Q.X*.65f)+FVector(0,0,Q.Y)+N*');put(p.name,s)
p=src/'HouseStadiumDetailLook.cpp';s=p.read_text()
start=s.index('void Line(FHallImage& I,');end=s.index('void Board(',start)
s=s[:start]+'''void Line(FHallImage& I,const FString& Text,float Y,float H,FLinearColor Ink,float Width=.90f)
{
 FHallTextStyle S;S.Height=H;S.Stroke=H*.085f;S.Spacing=.16f;S.Wobble=.38f;S.Seed=19861018;
 const FString Painted=Text.Replace(TEXT("\\u00a2"),TEXT("C"));S.WidthScale=FMath::Min(1.f,(I.W*Width)/FMath::Max(1.f,HallTextWidth(Painted,S)));
 const float Left=(I.W-HallTextWidth(Painted,S))*.5f;HallDrawText(I,Painted,{Left,Y},S,Ink);
 for(int32 J=0;J<Text.Len();++J)if(Text[J]==TCHAR(0xa2)){const float X=Left+(J?HallTextWidth(Painted.Left(J),S)+S.Spacing*H:0.f)+H*S.WidthScale/3.f;I.Stroke({{X+H*.07f,Y+H*.08f},{X-H*.07f,Y-H*1.08f}},S.Stroke*.8f,Ink);}
}
'''+s[end:]
for cents in (25,35,40,45,50,60,75):s=s.replace(str(cents)+'C',str(cents)+'\\u00a2')
marker='void HouseStadiumDetail::SignHardware('
start=s.index(' if(Polish){for(int32 I=0;I<UE_ARRAY_COUNT(Exterior);',s.index('void HouseStadiumDetail::SignLook'))
end=s.index(' UE_LOG(',start)
s=s[:start]+''' if(Polish){
  for(int32 I=0;I<UE_ARRAY_COUNT(Exterior);++I)Paint(Exterior[I],I+100);
  const TCHAR* Items[][6]={{TEXT("HOT DOGS"),TEXT("POPCORN"),TEXT("POP"),TEXT("COFFEE"),TEXT("CANDY"),TEXT("PEANUTS")},{TEXT("HOT CIDER"),TEXT("DONUTS"),TEXT("COFFEE"),TEXT("COCOA"),TEXT("CARAMEL APPLE"),TEXT("")}};
  const TCHAR* Prices[][6]={{TEXT("75\\u00a2"),TEXT("50\\u00a2"),TEXT("50\\u00a2"),TEXT("35\\u00a2"),TEXT("25\\u00a2"),TEXT("40\\u00a2")},{TEXT("50\\u00a2"),TEXT("3/$1"),TEXT("35\\u00a2"),TEXT("45\\u00a2"),TEXT("60\\u00a2"),TEXT("")}};
  for(int32 Menu=0;Menu<2;++Menu){FHallImage I;I.Init(768,960,Cream);
   ParallelFor(960,[&](int32 Y){for(int32 X=0;X<768;++X){I.At(X,Y)=Cream*(.94f+HallNoise(X*.4f,Y*.4f,Menu+1986)*.07f);}});
   I.Stroke({{16,16},{752,16},{752,944},{16,944},{16,16}},7,Purple);S::Line(I,Menu==0?TEXT("GEESE GRILL"):TEXT("CIDER MILL"),114,65,Purple);
   I.Stroke({{48,146},{720,146}},3,Purple);for(int32 Row=0;Row<(Menu==0?6:5);++Row){const float Y=250+Row*103.f;FHallTextStyle T;T.Height=38;T.Stroke=3.3f;T.Spacing=.12f;T.Wobble=.4f;T.WidthScale=.85f;HallDrawText(I,Items[Menu][Row],{52,Y},T,Dark);const FString Price=Prices[Menu][Row];FHallImage Small;Small.Init(176,76,Cream);S::Line(Small,Price,56,37,{.34f,.068f,.043f});for(int32 Py=0;Py<76;++Py)for(int32 Px=0;Px<176;++Px)I.At(542+Px,int32(Y)-56+Py)=Small.At(Px,Py);I.Stroke({{52,Y+22},{716,Y+22}},1,Gold,.5f);}
   S::Line(I,TEXT("CASH ONLY  -  THANK YOU CALDER"),900,25,Purple);HouseStadiumDetailLookImpl::Texture(Look,Menu==0?TEXT("CP_PriceBoardA"):TEXT("CP_PriceBoardB"),I,.83f,0,nullptr,false);
  }
 }else{for(int32 I=0;I<UE_ARRAY_COUNT(Owned);++I)Paint(Owned[I],I);}
'''+s[end:]
put(p.name,s)
# Additional optional native wall check. Wait for the actual streamed STA solid before tracing.
p=src/'HouseStadiumDetailReview.cpp';s=p.read_text();idx=s.index('void Tick(UWorld* W,')
s=s[:idx]+'''void CheckLockerSeams(UWorld* W)
{
 if(!HouseStadiumDetail::Enabled()||HouseStadiumDetail::Step()<6)return;
 static TWeakObjectPtr<UWorld> Checked;if(Checked.Get()==W)return;
 auto Point=[](float Degrees,float Radius,float Z){const int32 K=FMath::FloorToInt(Degrees/9.f);const float T=(Degrees-K*9.f)/9.f,A=K*PI/20,B=(K+1)*PI/20;return FVector(2400+FMath::Lerp(FMath::Cos(A),FMath::Cos(B),T)*5600*Radius,-13300+FMath::Lerp(FMath::Sin(A),FMath::Sin(B),T)*3800*Radius,Z);};
 FCollisionQueryParams Params(SCENE_QUERY_STAT(CalderStadiumWallSeam),true);if(auto* PC=W->GetFirstPlayerController())if(PC->GetPawn())Params.AddIgnoredActor(PC->GetPawn());
 const FVector End=Point(50,.76f,60),Along=(Point(50,.80f,0)-Point(50,.78f,0)).GetSafeNormal(),Into(-Along.Y,Along.X,0);FHitResult Ready;
 if(!W->LineTraceSingleByChannel(Ready,End+Into*60,End-Into*60,ECC_Pawn,Params)||!GetNameSafe(Ready.GetComponent()).Contains(TEXT("CampusInterior_STA_Solid")))return;
 Checked=W;int32 Rays=0,Leaks=0;
 for(float Degrees:{50.f,66.f}){const FVector Direction=(Point(Degrees,.80f,0)-Point(Degrees,.78f,0)).GetSafeNormal(),Normal=FVector(-Direction.Y,Direction.X,0)*(Degrees==50.f?1.f:-1.f);
  for(float Radius:{.696f,.700f,.740f,.800f,.840f,.880f,.884f})for(float Z:{-20.f,60.f,150.f}){const FVector C=Point(Degrees,Radius,Z);FHitResult H;++Rays;if(!W->LineTraceSingleByChannel(H,C+Normal*60,C-Normal*60,ECC_Pawn,Params)){++Leaks;UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_WALL_LEAK degrees=%.0f radius=%.3f z=%.1f"),Degrees,Radius,Z);}}
 }
 UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_WALL_SEAMS %s rays=%d leaks=%d actualStreamedRoom=1"),Leaks==0?TEXT("PASS"):TEXT("FAIL"),Rays,Leaks);
}
'''+s[idx:]
marker=' static TWeakObjectPtr<UWorld> Done;';assert s.count(marker)==1;s=s.replace(marker,' CheckLockerSeams(W);\n'+marker)
put(p.name,s)
print('Refined room surfaces, garment fit, complete price boards, cents glyphs and42-ray streamed-wall diagnostic.')
