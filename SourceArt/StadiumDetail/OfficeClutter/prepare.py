from pathlib import Path
import re
r=Path(__file__).resolve().parents[3];f=Path(__file__).parent
def rep(t,a,b):assert t.count(a)==1,a[:100];return t.replace(a,b,1)
t=(f/'HouseStadiumDetailInteriors.cpp.before').read_text()
# Robust convex containment also handles a point collinear with the extended first edge.
old=' double Sign=0;for(int32 J=0;J<4;++J){const FVector A=Poly[J],B=Poly[(J+1)%4];const double Cross=(B.X-A.X)*(P.Y-A.Y)-(B.Y-A.Y)*(P.X-A.X);if(J==0)Sign=Cross;else if(Sign*Cross<0)return false;}return true;'
new=' bool Positive=false,Negative=false;for(int32 J=0;J<4;++J){const FVector A=Poly[J],B=Poly[(J+1)%4];const double Cross=(B.X-A.X)*(P.Y-A.Y)-(B.Y-A.Y)*(P.X-A.X);Positive|=Cross>.01;Negative|=Cross<-.01;if(Positive&&Negative)return false;}return true;'
t=rep(t,old,new)
# Reuse a single notes atlas, with actual card overlap, independent angle and a lightly lifted corner.
k='void Wall(FHallMesh& Solid,FVector A,FVector B)'
i=t.index(k)
t=t[:i]+'''void Paper(FHallMesh& Mesh,FName Key,FVector Centre,FVector Across,FVector Up,FVector N,float Width,float Height,int32 Cell,float Degrees,float Curl=0)
{
 const float Angle=FMath::DegreesToRadians(Degrees);const FVector Right=Across*FMath::Cos(Angle)+Up*FMath::Sin(Angle),Rise=Up*FMath::Cos(Angle)-Across*FMath::Sin(Angle);
 const float Scale=Cell<0?1.f:1.f/3.f,U0=Cell<0?0.f:(Cell%3)*Scale,V0=Cell<0?0.f:(Cell/3)*Scale,Inset=.001f;
 Mesh.QuadUV(Key,Centre-Right*(Width*.5f)-Rise*(Height*.5f),Centre+Right*(Width*.5f)-Rise*(Height*.5f),Centre+Right*(Width*.5f)+Rise*(Height*.5f)+N*Curl,Centre-Right*(Width*.5f)+Rise*(Height*.5f),N,{U0+Inset,V0+Scale-Inset},{U0+Scale-Inset,V0+Scale-Inset},{U0+Scale-Inset,V0+Inset},{U0+Inset,V0+Inset},Right);
}
''' +t[i:]
# Give the desk its own slightly off-square frame while preserving measured walking and camera lanes.
a=t.index(' const FVector Desk=CoachPoint(');b=t.index(' // Four-spoke upholstered swivel chair',a);part=t[a:b]
part=part.replace('Desk=CoachPoint(145,-164,-55)','Desk=CoachPoint(153,-168,-55)')
for old,new in [('X','DX'),('Y','DY'),('R','DR')]:part=re.sub(r'(?<![.\w])'+old+r'\b',new,part)
part=' const FRotator DR=R+FRotator(0,3,0);const FVector DX=DR.Vector(),DY(-DX.Y,DX.X,0);\n'+part
part=rep(part,'P(0,2,77),{72,44,.55f},DR,.1f)','P(3,5,77),{72,44,.55f},DR+FRotator(0,-6,0),.1f)')
part=rep(part,'HouseCampusFallKit::Card(Detail,TEXT("SD_CoachRoster"),P(-8,1,77.4f),DX,DY,27,35,FVector::UpVector);','C::Paper(Detail,TEXT("SD_CoachRoster"),P(-9,1,77.52f),DX,DY,FVector::UpVector,27,35,-1,-7,.20f);')
old=' for(int32 J=0;J<4;++J)I::Box(Detail,TEXT("SD_CoachPaper"),P(37+J*.4f,13-J*.5f,77.3f+J*.14f),{22,29,.12f},DR);'
new=''' const float PageX[]={0,1.4f,-.8f,2.1f,-1.2f,.6f},PageY[]={0,-.9f,1.1f,-1.5f,.4f,1.6f},PageYaw[]={-9,3,-4,11,-1,6};
 for(int32 J=0;J<6;++J)I::Box(Detail,TEXT("SD_CoachPaper"),P(35+PageX[J],13+PageY[J],77.65f+J*.09f),{22,29,.10f},DR+FRotator(0,PageYaw[J],0));'''
part=rep(part,old,new)
# An open manila folder and paperwork partly overlap the skewed blotter and roster.
k=' // Desk blotter, carbon-copy roster, pencil, thermos and a generic rotary telephone. No modern brands or equipment.'
# Folder is inserted after the blotter but below the roster's actual z level.
pos=part.index(' C::Paper(Detail,TEXT("SD_CoachRoster")')
part=part[:pos]+''' C::Paper(Detail,TEXT("SD_CoachManila"),P(-4,4,77.35f),DX,DY,FVector::UpVector,35,39,-1,-2,.12f);
 C::Paper(Detail,TEXT("SD_CoachManila"),P(29,6,77.39f),DX,DY,FVector::UpVector,32,39,-1,2,.18f);
 Detail.Cylinder(TEXT("SD_FurnitureOakDark"),P(13,-15,77.43f),P(13,24,77.43f),.08f,5,FHallUV::World(100));
''' +part[pos:]
# Offset and turn the phone independently of the desktop.
x=part.index(' const FVector Phone=');y=part.index(' // Rubbed varnish',x);phone=part[x:y].replace('Phone=P(58,-12,76.5f)','Phone=P(60,-14,76.5f)')
for old,new in [('DX','PX'),('DY','PY'),('DR','PR')]:phone=re.sub(r'(?<![.\w])'+old+r'\b',new,phone)
phone=' const FRotator PR=DR+FRotator(0,-13,0);const FVector PX=PR.Vector(),PY(-PX.Y,PX.X,0);\n'+phone
part=part[:x]+phone+part[y:]
part=part.replace('Flask=P(-62,13,76.4f)','Flask=P(-55,18,76.4f)')
x=part.index(' const FVector Lamp=');lamp=part[x:]
for old,new in [('DX','LX'),('DY','LY'),('DR','LR')]:lamp=re.sub(r'(?<![.\w])'+old+r'\b',new,lamp)
lamp=lamp.replace('Lamp=P(-58,-23,76.4f)','Lamp=P(-63,-23,76.4f)')
lamp=' const FRotator LR=DR+FRotator(0,14,0);const FVector LX=LR.Vector(),LY(-LX.Y,LX.X,0);\n'+lamp
part=part[:x]+lamp
# Additional used-desk objects, all inside the retained desk footprint. Substantial mug surfaces collide.
part+=''' // Half-finished coffee, an addressed envelope, scrawled index cards, loose pens, paper clips and keys.
 const FVector Mug=P(-35,6,76.4f);HouseCampusFallKit::Lathe(Solid,TEXT("SD_Cream"),Mug,{{0,0},{3.5f,0},{4.1f,1},{4.2f,10.8f},{4.5f,11.6f},{3.7f,11.6f},{3.5f,1.4f},{0,1.4f}},24);
 Detail.Cylinder(TEXT("SD_LeatherCrack"),Mug+FVector(0,0,10.2f),Mug+FVector(0,0,10.3f),3.6f,24,FHallUV::World(100));
 for(int32 J=0;J<24;++J){auto Q=[&](int32 K){const float Angle=K*2*PI/24;return Mug+DX*(5.1f+FMath::Cos(Angle)*2.2f)+FVector(0,0,6+FMath::Sin(Angle)*3.4f);};Solid.Cylinder(TEXT("SD_Cream"),Q(J),Q(J+1),.6f,8,FHallUV::World(100));}
 I::Word(Detail,TEXT("CSU"),Mug+DY*4.3f+FVector(0,0,5),DX,DY,1.6f,TEXT("SD_LeatherCrack"));
 C::Paper(Detail,TEXT("SD_CoachClippings"),P(56,29,76.58f),DX,DY,FVector::UpVector,22,10,8,8,.20f);
 C::Paper(Detail,TEXT("SD_CoachClippings"),P(3,-25,76.57f),DX,DY,FVector::UpVector,14,9,1,-16,.12f);
 C::Paper(Detail,TEXT("SD_CoachClippings"),P(-15,25,77.55f),DX,DY,FVector::UpVector,12,8,2,13,.15f);
 Detail.Cylinder(TEXT("SD_Fabric"),P(23,-27,76.62f),P(35,-23,76.62f),.34f,8,FHallUV::World(100));Detail.Cylinder(TEXT("SD_CoachBrass"),P(34,-23.3f,76.62f),P(36,-22.7f,76.62f),.23f,8,FHallUV::World(100));
 Detail.Cylinder(TEXT("SD_Cream"),P(45,1,78.35f),P(49,12,78.35f),.30f,8,FHallUV::World(100));
 for(int32 J=0;J<3;++J){const FVector Clip=P(48+J*2.4f,-22+(J%2)*2.5f,76.55f);const float Angle=J*.7f;const FVector U=DX*FMath::Cos(Angle)+DY*FMath::Sin(Angle),V=DY*FMath::Cos(Angle)-DX*FMath::Sin(Angle);const FVector2D Wire[]={{-.4f,-.9f},{-.4f,.7f},{-.2f,1},{.3f,1},{.5f,.7f},{.5f,-.9f},{.2f,-1.1f},{-.1f,-1.1f},{-.2f,-.8f},{-.2f,.5f}};for(int32 K=1;K<UE_ARRAY_COUNT(Wire);++K)Detail.Cylinder(TEXT("SD_Aluminium"),Clip+U*Wire[K-1].X+V*Wire[K-1].Y,Clip+U*Wire[K].X+V*Wire[K].Y,.065f,5,FHallUV::World(100));}
 const FVector Keys=P(-16,-29,76.55f);for(int32 J=0;J<20;++J){const float A0=J*2*PI/20,A1=(J+1)*2*PI/20;Detail.Cylinder(TEXT("SD_CoachBrass"),Keys+DX*(FMath::Cos(A0)*1.5f)+DY*(FMath::Sin(A0)*1.5f),Keys+DX*(FMath::Cos(A1)*1.5f)+DY*(FMath::Sin(A1)*1.5f),.12f,6,FHallUV::World(100));}
 for(int32 J=0;J<2;++J){const FVector Tip=Keys+DX*(J?3.7f:3.1f)+DY*(J?2.8f:-2.1f);Detail.Cylinder(TEXT("SD_CoachBrass"),Keys,Tip,.22f,8,FHallUV::World(100));I::Box(Detail,TEXT("SD_CoachBrass"),Tip,{1.4f,.8f,.18f},DR+FRotator(0,J?31:-27,0));}
 HouseCampusFallKit::Ball(Detail,TEXT("SD_CoachPaper"),P(70,25,78.7f),2.3f,7);
'''
t=t[:a]+part+t[b:]
# Both chairs sit at plausible independent angles. Keep the proven through-lane clear.
a=t.index(' const FVector Chair=');b=t.index(' // Visitor chair sits',a);part=t[a:b].replace('Chair=CoachPoint(180,-262,-55)','Chair=CoachPoint(185,-264,-55)')
for old,new in [('X','CX'),('Y','CY'),('R','CR')]:part=re.sub(r'(?<![.\w])'+old+r'\b',new,part)
part=' const FRotator CR=R+FRotator(0,-9,0);const FVector CX=CR.Vector(),CY(-CX.Y,CX.X,0);\n'+part;t=t[:a]+part+t[b:]
t=rep(t,' const FVector Guest=CoachPoint(-75,-135,-55),GX=Y,GY=-X;const FRotator GR=GX.Rotation();',' const FVector Guest=CoachPoint(-69,-135,-55);const FRotator GR=R+FRotator(0,-101,0);const FVector GX=GR.Vector(),GY(-GX.Y,GX.X,0);')
# Layer notes, tickets and the fictional snapshot in front of the existing newspaper/notes collage.
i=t.index(' // Brass doorplate is beside the opening')
t=t[:i]+''' // Owner-requested bulletin-board clutter: deliberately overlapped and pinned at different angles.
 C::Paper(Detail,TEXT("SD_CoachSnapshot"),Board-BackX*15+FVector(0,0,20)+BackN*3.6f,BackX,FVector::UpVector,BackN,40,26,-1,-5,.25f);
 struct FNote{float U,V,W,H,Angle;int32 Cell;};const FNote Notes[]={{34,23,20,23,-7,1},{5,-35,28,10,11,0},{-40,-22,20,15,8,2},{11,-9,22,28,-5,3},{38,-19,18,18,12,5},{-14,-15,23,10,-13,6},{-40,26,18,22,-9,4},{28,1,17,12,6,8}};
 int32 Sheet=0;for(const FNote& Note:Notes){const FVector Centre=Board+BackX*Note.U+FVector(0,0,Note.V)+BackN*(3.9f+Sheet*.11f);C::Paper(Detail,TEXT("SD_CoachClippings"),Centre,BackX,FVector::UpVector,BackN,Note.W,Note.H,Note.Cell,Note.Angle,.30f+(Sheet%3)*.15f);const float A0=FMath::DegreesToRadians(Note.Angle);const FVector Up=FVector::UpVector*FMath::Cos(A0)-BackX*FMath::Sin(A0),Pin=Centre+Up*(Note.H*.5f-1)+BackN*.15f;Detail.Cylinder(Sheet%3==0?TEXT("SD_CoachBrass"):TEXT("SD_Rust"),Pin,Pin+BackN*.5f,.65f,10,FHallUV::World(100));++Sheet;}
 for(float U:{-30.f,1.f}){const FVector Pin=Board+BackX*U+FVector(0,0,30)+BackN*4;Detail.Cylinder(TEXT("SD_CoachBrass"),Pin,Pin+BackN*.6f,.65f,10,FHallUV::World(100));}
''' +t[i:]
(f/'HouseStadiumDetailInteriors.cpp.candidate').write_text(t)
# One efficient shared note atlas and two very small additional paper materials.
t=(f/'HouseStadiumDetailLook.cpp.before').read_text();i=t.index('  FHallImage Roster;')
extra='''  HallMakePlain(Look,TEXT("SD_CoachManila"),{.36f,.255f,.105f},.98f);
  if(auto* Snapshot=LoadObject<UTexture2D>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Stadium/Textures/T_Stadium_CoachSnapshot.T_Stadium_CoachSnapshot")))HallMakeTextured(Look,TEXT("SD_CoachSnapshot"),Snapshot,nullptr,.97f,{.75f,.73f,.69f},0);else Look->Materials.Add(TEXT("SD_CoachSnapshot"),Look->Mat(TEXT("SD_CoachPaper")));
  FHallImage Clippings;Clippings.Init(768,768,{.40f,.34f,.23f});
  const TCHAR* Lines[9][4]={{TEXT("SAT OCT 18"),TEXT("24 - 17"),TEXT("KEEP THIS ONE"),TEXT("GO GEESE")},{TEXT("PRACTICE"),TEXT("MONDAY 6 AM"),TEXT("NO EXCUSES"),TEXT("- COACH")},{TEXT("LOST KEYS"),TEXT("BRASS RING"),TEXT("CHECK THE"),TEXT("BUS BAY")},{TEXT("DUNMORE"),TEXT("SCOUTING"),TEXT("BLITZ ON"),TEXT("THIRD DOWN")},{TEXT("TEAM BUS"),TEXT("LEAVES 6:15"),TEXT("AWAY GAME"),TEXT("DO NOT BE LATE")},{TEXT("EQUIPMENT"),TEXT("RETURN ALL"),TEXT("BY MONDAY"),TEXT("THANK YOU")},{TEXT("CALL"),TEXT("MAINTENANCE"),TEXT("LEAK BY"),TEXT("TUNNEL")},{TEXT("1978 TEAM"),TEXT("LEAGUE"),TEXT("CHAMPIONS"),TEXT("CALDER STATE")},{TEXT("TO COACH"),TEXT("THANK YOU"),TEXT("SENIORS"),TEXT("CLASS OF 86")}};
  for(int32 Cell=0;Cell<9;++Cell){FHallImage Note;const FLinearColor Base=Cell%3==0?FLinearColor(.48f,.40f,.25f):Cell%3==1?FLinearColor(.52f,.43f,.20f):FLinearColor(.33f,.39f,.37f);Note.Init(256,256,Base);for(int32 Y=0;Y<256;++Y)for(int32 X=0;X<256;++X){const float Grain=HallNoise(X*1.2f,Y*1.2f,1931+Cell),Stain=HallFbm(X*.025f,Y*.02f,3,1978+Cell);Note.At(X,Y)=Base*(.79f+Stain*.25f+Grain*.055f);}for(int32 Line=0;Line<4;++Line){FHallTextStyle Style;Style.Height=Line==0?21:17;Style.Stroke=Style.Height*.065f;Style.Wobble=Cell%2?1.1f:.15f;Style.Seed=1986+Cell*7+Line;const FString Text(Lines[Cell][Line]);HallDrawText(Note,Text,{(256-HallTextWidth(Text,Style))*.5f,52.f+Line*47},Style,{.055f,.04f,.025f});}Note.Stroke({{18,71},{238,71}},.7f,{.18f,.13f,.07f});for(int32 Y=0;Y<256;++Y)for(int32 X=0;X<256;++X)Clippings.At((Cell%3)*256+X,(Cell/3)*256+Y)=Note.At(X,Y);}
  L::Texture(Look,TEXT("SD_CoachClippings"),Clippings,.98f,0,nullptr,false);
'''
t=t[:i]+extra+t[i:];(f/'HouseStadiumDetailLook.cpp.candidate').write_text(t)
print('Prepared natural desk/chair angles, overlapping clutter, mug/keys/stationery and layered board/photo outside runtime.')
