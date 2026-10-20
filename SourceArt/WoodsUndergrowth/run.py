import pathlib,sys,subprocess,shutil,json,time
W=pathlib.Path(__file__).resolve().parent;R=W.parents[1];tag=sys.argv[1];U=pathlib.Path('/tmp/woods-undergrowth-'+tag);U.mkdir(exist_ok=True);E=W/'evidence'
assert subprocess.run(['pgrep','-x','UnrealEditor'],capture_output=True).returncode!=0
flags={'views':['-HouseVisualReview','-HouseVisualBenchmark','-HouseVisualQuality=2','-HouseVisualLayout='+str(W/'views.json'),'-CampusWoodsReview','-WoodsSceneReview','-WoodsUndergrowthReview'], 'polish':['-HouseVisualReview','-HouseVisualQuality=2','-HouseVisualLayout='+str(W/'polish-views.json'),'-WoodsSceneReview','-WoodsUndergrowthReview'], 'court':['-HouseBasketballReview'], 'walk':['-WoodsWalkReview','-WoodsUndergrowthReview']}[tag]
cmd=['/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor',str(R/'SeniorSendoff.uproject'),'/Game/Story/Maps/Chapter01_House','-game','-windowed','-ResX=1280','-ResY=720','-NoSplash','-NoTraceServer','-UserDir='+str(U),'-AbsLog='+str(E/(tag+'.log'))]+flags+['-ExecCmds=t.MaxFPS 0,r.VSync 0']
start=time.monotonic()
with (E/(tag+'-stdout.log')).open('w')as f:
 p=subprocess.Popen(cmd,stdout=f,stderr=subprocess.STDOUT)
 try:code=p.wait(timeout=300)
 except subprocess.TimeoutExpired:p.terminate();code=p.wait(timeout=20)
if (U/'Saved').exists():shutil.copytree(U/'Saved',E/tag,dirs_exist_ok=True)
log=(E/(tag+'.log')).read_text(errors='replace')
markers={'views':['CAMPUS_WOODS_REVIEW PASS','WOODS_REVIEW PASS','HOUSE_VISUAL_REVIEW_PASSED'], 'polish':['WOODS_REVIEW PASS','HOUSE_VISUAL_REVIEW_PASSED'], 'court':['COURT_REVIEW_PASS'], 'walk':['WOODS_REVIEW PASS']}[tag]
passed=code==0 and all(m in log for m in markers)
(E/(tag+'-run.json')).write_text(json.dumps(dict(exit_code=code,seconds=time.monotonic()-start,passed=passed,expected_markers=markers),indent=2));print('PASS' if passed else 'FAIL',code,flush=True)
sys.exit(0 if passed else 1)
