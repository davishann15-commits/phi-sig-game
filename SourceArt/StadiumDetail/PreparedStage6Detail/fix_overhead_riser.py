from pathlib import Path
import sys,json
ROOT=Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT/'SourceArt/StadiumDetail'));from stage_runner import idle,manifest
idle();src=ROOT/'Source/SeniorSendoff';draft=ROOT/'SourceArt/StadiumDetail/Draft';before=manifest()
def put(name,s):
 for p in [src/name,draft/name]:p.unlink();p.write_text(s)
p=src/'HouseStadiumDetailSeating.cpp';s=p.read_text();m='S::Panel(M,TEXT("SD_Concrete"),P(R1,T0,0),P(R1,T1,0),Z-63,Z,Out);';assert s.count(m)==1
n='''if(Row==0&&Step()>=6&&K>=5&&K<=7){
 // The inherited rear riser formerly cut through the real locker room atZ117..180.
 // Retain its outside portions and all seating/floors; close only above the175cm room ceiling.
 const float Start=FMath::Clamp((50.f-K*9.f)/9.f,T0,T1),End=FMath::Clamp((66.f-K*9.f)/9.f,T0,T1);
 if(Start>T0)S::Panel(M,TEXT("SD_Concrete"),P(R1,T0,0),P(R1,Start,0),Z-63,Z,Out);
 if(End>Start)S::Panel(M,TEXT("SD_Concrete"),P(R1,Start,0),P(R1,End,0),175,Z,Out);
 if(T1>End)S::Panel(M,TEXT("SD_Concrete"),P(R1,End,0),P(R1,T1,0),Z-63,Z,Out);
 }else '''+m
s=s.replace(m,n);put(p.name,s)
p=src/'HouseStadiumDetailReview.cpp';s=p.read_text();marker='  if(HouseStadiumDetail::Step()>=6){MinTread=';assert s.count(marker)==1
s=s.replace(marker,'''  if(HouseStadiumDetail::Step()>=6){
   // Additional full-height path from the room through the existing players' tunnel.
   // Original STA tour waypoints remain byte-identical; these checks add the actual passage.
   FVector Previous;for(int32 J=0;J<=14;++J){const float R=.79f-J*.01f;const FVector P=Mid(6,R,R>=.69f?-55.f:-56.f);CheckFloor(P,TEXT("locker-to-field-tunnel"));if(J){FHitResult H;++Sweeps;if(W->SweepSingleByChannel(H,Previous+FVector(0,0,97),P+FVector(0,0,97),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(36,96),Q)){++Blocked;OK=false;if(Problems.Num()<32)Problems.Add(FString::Printf(TEXT("tunnel full-height sweep blocked start=%s end=%s component=%s"),*Previous.ToString(),*P.ToString(),*GetNameSafe(H.GetComponent())));}}Previous=P;}
  }
'''+marker)
put(p.name,s)
p=src/'HouseStadiumDetailLook.cpp';s=p.read_text();marker=' }else{for(int32 I=0;I<UE_ARRAY_COUNT(Owned);++I)Paint(Owned[I],I);}'
assert s.count(marker)==1
s=s.replace(marker,'''  for(int32 Sign=0;Sign<2;++Sign){FHallImage I;I.Init(1024,256,{.69f,.61f,.41f});for(int32 J=-1;J<12;++J){const float X=J*96.f;for(float Y:{0.f,224.f})I.FillPolygon({{X,Y},{X+43,Y},{X+75,Y+32},{X+32,Y+32}},{.55f,.22f,.035f});}
   S::Line(I,Sign==0?TEXT("CALDER STATE PHYSICAL PLANT"):TEXT("BUS LOADING - NO PARKING"),129,53,Dark);S::Line(I,TEXT("STADIUM OPERATIONS"),198,25,Dark);HouseStadiumDetailLookImpl::Texture(Look,Sign==0?TEXT("CP_Sawhorse"):TEXT("CP_SawhorseBus"),I,.79f,0,nullptr,false);
  }
'''+marker);put(p.name,s)
(ROOT/'Saved/StadiumDetailWork/stage6_a4_overhead_and_tunnel.json').write_text(json.dumps({'before':before,'after':manifest(),'evidence':'a3 actual collision rays hit originalExteriorNear_40 section8 atZ142.747/158.660 andr~.714; inherited row0 rear riser crosses room airspace.','change':'Only enabled stage6+ rear face in the50..66degree locker-room span is truncated below175ceiling; outside pieces and every seating/floor position remain. Extra15floor/14full-height capsule checks traverse original tunnel to field. Traffic signs improved as requested.','native_validation_pending':True},indent=2)+'\n')
print('Localized riser clearance and stronger tunnel checks installed; native proof required.')
