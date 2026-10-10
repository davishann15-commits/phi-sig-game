import subprocess,pathlib,json,time,sys
ROOT=pathlib.Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff')
OUT=ROOT/'SourceArt/HouseScale110/evidence'
UE='/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
CASES=[
 ('doors',['-ExistingHouseDoorSmoke','-ExistingHouseDoorTargets=SM_OriginalLeaf_Upper_m1_p0_Solid_01,Porch_p0_p0_01,SM_OriginalLeaf_Upper_p0_m2_Solid_04,BasementLibrary_Mechanical'],290),
 ('water',['-HouseHotTubWaterSmoke'],160),
 ('pigs',['-HousePigSmoke','-HousePigResizeSmoke'],160),
 ('vents',['-HouseVentSmoke'],210),
 ('stairs',['-HouseStairSurvey','-StairWallWalk'],160),
 ('hallway',['-HouseImpossibleHallwaySmoke'],300),
 ('views',['-HouseVisualReview','-HouseFloorPorchCheck','-HouseVisualLayout='+str(ROOT/'SourceArt/HallFloorsPorch/final_views.json'),'-HouseVisualQuality=1'],210),
 ('rooms',['-HouseVisualReview','-HouseVisualLayout='+str(ROOT/'SourceArt/HouseScale110/room_views.json'),'-HouseVisualQuality=1'],180),
 ('porch',['-HouseTraversalSmoke'],150)
]
MARKERS={'doors':'EXISTING_HOUSE_DOOR_PASSED:', 'water':'HOUSE_HOT_TUB_WATER_SMOKE_RESULT passed=1', 'pigs':'HOUSE_PIG_RESIZE_SMOKE_RESULT passed=1', 'vents':'HOUSE_VENT_SMOKE_PASSED', 'stairs':'STAIR_WALL_WALK_PASS', 'hallway':'HOUSE_IMPOSSIBLE_HALLWAY_SMOKE_RESULT passed=1', 'views':'HOUSE_VISUAL_REVIEW_PASSED','rooms':'HOUSE_VISUAL_REVIEW_PASSED','porch':'HOUSE_TRAVERSAL_PASSED'}
results=[]
for name,flags,limit in CASES:
 if len(sys.argv)>1 and name not in sys.argv[1:]:continue
 user=pathlib.Path('/tmp/house-scale-'+name);user.mkdir(exist_ok=True)
 log=OUT/(name+'.log');start=time.time()
 print('START '+name,flush=True)
 cmd=[UE,str(ROOT/'SeniorSendoff.uproject'),'/Game/Story/Maps/Chapter01_House','-game','-windowed','-ResX=1280','-ResY=720','-nosound','-UserDir='+str(user),'-AbsLog='+str(log)]+flags
 with open(OUT/(name+'-stdout.log'),'w') as stream:
  proc=subprocess.Popen(cmd,stdout=stream,stderr=subprocess.STDOUT)
  try:rc=proc.wait(timeout=limit)
  except subprocess.TimeoutExpired:
   proc.terminate()
   try:proc.wait(timeout=15)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
   rc='TIMEOUT'
 text=log.read_text(errors='replace') if log.exists() else ''
 result=dict(case=name,status='PASS' if MARKERS[name] in text else 'FAIL',exit_code=rc,seconds=round(time.time()-start,2),log=str(log),user_dir=str(user))
 results.append(result)
 json.dump(results,open(OUT/'runtime-results.json','w'),indent=2)
 print('FINISH '+name+' '+str(rc),flush=True)
print('COMPLETE',flush=True)
