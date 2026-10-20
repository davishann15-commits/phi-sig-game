from pathlib import Path
import json,hashlib
root=Path(__file__).resolve().parents[3];folder=Path(__file__).parent;src=root/'Source/SeniorSendoff';draft=root/'SourceArt/StadiumDetail/Draft'
def edit(name,fn):
 p=src/name;before=p.read_text();assert before==(folder/(name+'.before')).read_text(),name+' changed since office preparation';out=fn(before);assert out!=before
 p.unlink();p.write_text(out)
 if (draft/name).exists():(draft/name).unlink();(draft/name).write_text(out)
 return {'before':hashlib.sha256(before.encode()).hexdigest(),'after':hashlib.sha256(out.encode()).hexdigest()}
def replace(s,old,new):assert s.count(old)==1,old[:160];return s.replace(old,new,1)
receipt={}
receipt['HouseStadiumDetail.h']=edit('HouseStadiumDetail.h',lambda s:replace(s,'void RoomIdentity(FHallMesh& Solid,FHallMesh& Detail);','void RoomIdentity(FHallMesh& Solid,FHallMesh& Detail);\nFVector CoachPoint(float U,float V,float WorldZ);\nbool CoachInside(FVector P);\nvoid CoachPartition(FHallMesh& Solid,FVector Start,FVector End,FVector Along,FVector Side,float Shift);\nvoid CoachOffice(FHallMesh& Solid,FHallMesh& Detail,FHallMesh& Glow);'))
def interiors(s):
 start=s.index(' const FVector Base=I::At(50.f,.735f,0)+Into*23;');end=s.index(' const FVector Pipe=I::At(50.f,.715f,0)+Into*23;',start)
 s=s[:start]+''' // Split the faded stripe around the coach-office opening; no trim or paper spans the doorway.
 for(FVector2D Range:{FVector2D(-295,-62),FVector2D(62,600)}){const FVector Stripe=CoachPoint((Range.X+Range.Y)*.5f,22.2f,133);HouseCampusFallKit::Card(Detail,TEXT("SD_RoomStripe"),Stripe,Along,FVector::UpVector,Range.Y-Range.X,13,Into);}
'''+s[end:]
 start=s.index(' const FVector Desk=I::At(52.5f,.73f,-55.f);');end=s.index(' // Two off-white towels',start);s=s[:start]+s[end:]
 s=replace(s,'void HouseStadiumDetail::Interiors(FHallMesh& Solid,FHallMesh& Clutter,FHallMesh&)','void HouseStadiumDetail::Interiors(FHallMesh& Solid,FHallMesh& Clutter,FHallMesh& Glow)')
 s=replace(s,' RoomIdentity(Solid,Clutter);',' RoomIdentity(Solid,Clutter);\n CoachOffice(Solid,Clutter,Glow);')
 s=replace(s,'trainer=1 whirlpool=1 tunnelPads=2 newRooms=0 newLights=0','trainer=1 whirlpool=1 tunnelPads=2 coachOffice=1 ownerAuthorizedRoom=1 newLights=0')
 geometry=(folder/'geometry.inc').read_text().replace('Guest=CoachPoint(-40,-150,-55)','Guest=CoachPoint(-75,-135,-55)')
 return s+'\n'+geometry
receipt['HouseStadiumDetailInteriors.cpp']=edit('HouseStadiumDetailInteriors.cpp',interiors)
def legacy(s):
 key=' const FVector Start=A-Along*30,End=B+Along*35;const float Length=FVector::Dist(Start,End);'
 s=replace(s,key,key+'\n if(HouseStadiumDetail::Step()>=7&&Degrees==StaLockA0){HouseStadiumDetail::CoachPartition(M,Start,End,Along,Side,Shift);return;}')
 s=s.replace('StaAt(A, .79f, StaGround + 135.f)','StaAt(A, HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=7?.805f:.79f, StaGround + 135.f)')
 s=s.replace('StaAt(A, .79f, StaGround + 211.f)','StaAt(A, HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=7?.805f:.79f, StaGround + 211.f)')
 start=s.index('    const FVector Desk = StaAt(FMath::DegreesToRadians(52.5f)');end=s.index('\n}',start)
 old=s[start:end];s=s[:start]+'    if(!HouseStadiumDetail::Enabled()||HouseStadiumDetail::Step()<7){\n'+old+'\n    }'+s[end:]
 s=replace(s,'bool StaInside(const FVector& P)\n{','bool StaInside(const FVector& P)\n{\n    if(HouseStadiumDetail::CoachInside(P))return true;')
 s=replace(s,'void StaLightAnchors(int32 Seed, float Height, TArray<FVector>& Out)\n{','void StaLightAnchors(int32 Seed, float Height, TArray<FVector>& Out)\n{\n    if(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=7)Out.Add(HouseStadiumDetail::CoachPoint(100,-160,130));')
 key='    ChordDoor(TEXT("WomensRoom"), 9, 534.f, 626.f, TEXT("WOMEN - CLOSED FOR CLEANING AFTER THE GAME"), false);'
 s=replace(s,key,key+'''
    if(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=7){
        const FVector Along=(HouseStadiumDetail::CoachPoint(1,0,0)-HouseStadiumDetail::CoachPoint(0,0,0)).GetSafeNormal();
        FDoorSpec Office{TEXT("CoachOffice"),HouseStadiumDetail::CoachPoint(-55,2,-55),Along.Rotation().Yaw,EHallDoorStyle::Equipment,110.f,-1.f,uint8(AHouseBlackHallDoor::Lever|AHouseBlackHallDoor::KickPlate),HouseStadiumDetail::CoachPoint(0,10,50),NAME_None,FString()};
        Office.Height=210;Office.bOpen=true;Out.Add(Office);
    }''')
 return s
receipt['HouseCampusInteriorsStadium.cpp']=edit('HouseCampusInteriorsStadium.cpp',legacy)
def look(s):
 index=s.index('\n  FHallImage Stripe;')
 extra='''
  HallMakePlain(Look,TEXT("SD_CoachEnamel"),{.12f,.15f,.115f},.72f,.38f);
  FHallImage Cloth,ClothN;Cloth.Init(256,256,{.115f,.052f,.028f});TArray<float> H;H.SetNum(256*256);
  ParallelFor(256,[&](int32 Y){for(int32 X=0;X<256;++X){const float G=HallNoise(X*1.7f,Y*1.7f,1981),Age=HallNoise(X*.025f,Y*.04f,1948);Cloth.At(X,Y)=FLinearColor(.115f,.052f,.028f)*(.8f+Age*.3f+G*.1f);H[Y*256+X]=G*.045f;}});HallHeightToNormal(H,256,256,.3f,true,ClothN);L::Texture(Look,TEXT("SD_CoachUpholstery"),Cloth,.83f,0,&ClothN);
  FHallImage Roster;Roster.Init(512,768,{.65f,.60f,.45f});L::Label(Roster,TEXT("CALDER STATE"),60,34,{.075f,.06f,.045f});L::Label(Roster,TEXT("FOOTBALL"),110,28,{.075f,.06f,.045f});L::Label(Roster,TEXT("OCTOBER 18 1986"),155,21,{.075f,.06f,.045f});L::Label(Roster,TEXT("POSTGAME NOTES"),220,27,{.075f,.06f,.045f});
  const TCHAR* Rows[]={TEXT("12 WARD      QB"),TEXT("24 BELL      HB"),TEXT("37 REED      FB"),TEXT("41 FINCH     LB"),TEXT("56 MASON     C"),TEXT("63 GREER     G"),TEXT("78 COLE      T")};for(int32 J=0;J<7;++J){L::Label(Roster,Rows[J],290+J*46,22,{.10f,.085f,.06f});Roster.Stroke({{45,300.f+J*46},{467,300.f+J*46}},.7f,{.34f,.31f,.24f});}L::Label(Roster,TEXT("RETURN EQUIPMENT MONDAY"),680,20,{.17f,.085f,.055f});L::Texture(Look,TEXT("SD_CoachRoster"),Roster,.95f,0,nullptr,false);
'''
 return s[:index]+extra+s[index:]
receipt['HouseStadiumDetailLook.cpp']=edit('HouseStadiumDetailLook.cpp',look)
views=json.loads((folder/'office_views.json').read_text())
def cameras(s):
 key='    { TEXT("detail_stadium_programs"), FVector(-485.863,-15722.141,710.000), FVector(-451.251,-15776.403,495.180), 75.f },'
 lines=['    // '+views['added_comment']]
 for v in views['views']:
  vec=lambda k:','.join(f'{n:.3f}' for n in v[k]);lines.append('    { TEXT("'+v['name']+'"), FVector('+vec('eye')+'), FVector('+vec('target')+'), 75.f },')
 (draft/'office_views.inl').write_text('\n'.join(lines)+'\n')
 return replace(s,key,key+'\n'+'\n'.join(lines))
receipt['HouseExterior.cpp']=edit('HouseExterior.cpp',cameras)
(folder/'install_receipt.json').write_text(json.dumps({'installed':True,'compiled':False,'native_tested':False,'scope':'Owner-authorized connected office and furniture; all original paths and camera poses preserved.','files':receipt},indent=2)+'\n')
print('Installed five source files and matching owned Draft. Build/native inspection pending.')
