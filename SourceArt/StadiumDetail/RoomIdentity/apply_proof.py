from pathlib import Path
import sys,hashlib,json
ROOT=Path(__file__).resolve().parents[3];ART=ROOT/'SourceArt/StadiumDetail';D=Path(__file__).resolve().parent;SRC=ROOT/'Source/SeniorSendoff'
sys.path.insert(0,str(ART));from stage_runner import idle,manifest
idle();before=manifest()
for name in ('HouseStadiumDetailLook.cpp','HouseStadiumDetailInteriors.cpp','HouseStadiumDetail.h','HouseCampusInteriorsStadium.cpp'):assert (D/(name+'.before')).read_bytes()==(SRC/name).read_bytes(),name
changes={}
def edit(name,old,new):
 s=changes.get(name,(SRC/name).read_text());assert s.count(old)==1,(name,old[:80]);changes[name]=s.replace(old,new,1)
condition='HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=7'
edit('HouseStadiumDetail.h','void SignLook(UHouseBlackHallSubsystem* Look,bool Polish);','void RoomFixture(FHallMesh& Glow,FHallMesh& Detail,FVector Centre,FVector Along);\nvoid RoomIdentity(FHallMesh& Solid,FHallMesh& Detail);\nvoid SignLook(UHouseBlackHallSubsystem* Look,bool Polish);')
edit('HouseStadiumDetailLook.cpp',' UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_SURFACE_PREP lookParallel=',(D/'look.inc').read_text()+'\n UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_SURFACE_PREP lookParallel=')
edit('HouseStadiumDetailInteriors.cpp','void HouseStadiumDetail::Interiors(FHallMesh& Solid,FHallMesh& Clutter,FHallMesh&)',(D/'geometry.inc').read_text()+'\nvoid HouseStadiumDetail::Interiors(FHallMesh& Solid,FHallMesh& Clutter,FHallMesh&)')
edit('HouseStadiumDetailInteriors.cpp',' UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_INTERIOR sameDoors=1',' RoomIdentity(Solid,Clutter);\n UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_INTERIOR sameDoors=1')
name='HouseCampusInteriorsStadium.cpp'
edit(name,'M.Box(TEXT("CI_StaBlockPurple"),FTransform(Along.Rotation(),(Start+End)',f'M.Box({condition}?TEXT("SD_RoomPlaster"):TEXT("CI_StaBlockPurple"),FTransform(Along.Rotation(),(Start+End)')
edit(name,'{Length*.5f,10,(StaLockCeil+2-StaGround)*.5f},FHallUV::World(80));','{Length*.5f,10,(StaLockCeil+2-StaGround)*.5f},FHallUV::World(HouseStadiumDetail::Step()>=7?200:80));')
edit(name,'StaBand(Solid, TEXT("CI_StaRubberFloor"), Pt.K, StaLockR0, StaLockR1, Pt.T0, Pt.T1, StaFloor, true, FHallUV::World(120.f));',f'StaBand(Solid, {condition}?TEXT("SD_RoomFloor"):TEXT("CI_StaRubberFloor"), Pt.K, StaLockR0, StaLockR1, Pt.T0, Pt.T1, StaFloor, true, FHallUV::World({condition}?250.f:120.f));')
edit(name,'StaBand(Solid, TEXT("CI_CeilingTile"), Pt.K, StaLockR0, StaLockR1, Pt.T0, Pt.T1, StaLockCeil, false, FHallUV::World(122.f));',f'StaBand(Solid, {condition}?TEXT("SD_RoomCeiling"):TEXT("CI_CeilingTile"), Pt.K, StaLockR0, StaLockR1, Pt.T0, Pt.T1, StaLockCeil, false, FHallUV::World({condition}?240.f:122.f));')
edit(name,'I.Wall(Solid, TEXT("CI_StaBlockPurple"), I.Len * Pt.T0, I.Len * Pt.T1, -22.f, 0.f, StaGround, StaLockCeil + 2.f, Holes, FHallUV::World(80.f));',f'I.Wall(Solid, {condition}?TEXT("SD_RoomPlaster"):TEXT("CI_StaBlockPurple"), I.Len * Pt.T0, I.Len * Pt.T1, -22.f, 0.f, StaGround, StaLockCeil + 2.f, Holes, FHallUV::World({condition}?200.f:80.f));')
edit(name,'O.Wall(Solid, TEXT("CI_StaBlockPurple"), O.Len * Pt.T0, O.Len * Pt.T1, 0.f, StaWallT, StaGround, StaLockCeil + 2.f, {}, FHallUV::World(80.f));',f'O.Wall(Solid, {condition}?TEXT("SD_RoomPlaster"):TEXT("CI_StaBlockPurple"), O.Len * Pt.T0, O.Len * Pt.T1, 0.f, StaWallT, StaGround, StaLockCeil + 2.f, {{}}, FHallUV::World({condition}?200.f:80.f));')
edit(name,'Troffer(Glow, Clutter, FVector(P.X, P.Y, StaLockCeil - .6f), false);',f'if({condition})HouseStadiumDetail::RoomFixture(Glow,Clutter,FVector(P.X,P.Y,StaLockCeil-.6f),I.Along);else Troffer(Glow, Clutter, FVector(P.X, P.Y, StaLockCeil - .6f), false);')
edit(name,'for (int32 I = 0; I < 26; ++I)',f'for (int32 I = 0; I < ({condition}?14:26); ++I)')
edit(name,'for (int32 I = 0; I < 4; ++I)\n        Kit.Floor(EKit::BunchedTowel',f'for (int32 I = 0; I < ({condition}?0:4); ++I)\n        Kit.Floor(EKit::BunchedTowel')
for name,text in changes.items():
 p=SRC/name;p.unlink();p.write_text(text)
 if name.startswith('HouseStadiumDetail'):
  p=ART/'Draft'/name;p.unlink();p.write_text(text)
(D/'proof_install.json').write_text(json.dumps({'installed':True,'source_before':before,'source_after':manifest(),'stage':7,'scope':'Room materials/ceiling utilities and one coach corner proof only; fresh actual image/native review mandatory. No prior camera or route changed.'},indent=2)+'\n')
print('Room identity proof installed; compile and native preview required.')
