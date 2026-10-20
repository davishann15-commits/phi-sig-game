import pathlib,subprocess,shutil,json,time,sys
B=pathlib.Path(__file__).resolve().parent;R=B.parents[1];E=B/'evidence';tag=sys.argv[1];U=pathlib.Path('/tmp/surroundings-'+tag);U.mkdir(exist_ok=True)
assert subprocess.run(['pgrep','-x','UnrealEditor'],capture_output=True).returncode!=0
benchmark=tag in ['baseline','after'];layout='benchmark-views.json' if benchmark else 'views.json'
cmd=['/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor',str(R/'SeniorSendoff.uproject'),'/Game/Story/Maps/Chapter01_House','-game','-windowed','-ResX=1280','-ResY=720','-NoSplash','-NoTraceServer','-UserDir='+str(U),'-AbsLog='+str(E/(tag+'.log')),'-HouseVisualReview','-HouseVisualQuality=2','-HouseVisualLayout='+str(B/layout),'-CampusWoodsReview','-FraternityBoundaryReview','-WoodsSceneReview','-WoodsUndergrowthReview','-ExecCmds=t.MaxFPS 0,r.VSync 0']
if benchmark:cmd+=['-HouseVisualBenchmark']
if tag=='baseline':cmd+=['-NoSurroundingsPolish']
if tag!='baseline':cmd+=['-SurroundingsPolishReview']
t=time.monotonic()
with (E/(tag+'-stdout.log')).open('w')as f:
 p=subprocess.Popen(cmd,stdout=f,stderr=subprocess.STDOUT)
 try:code=p.wait(timeout=300)
 except subprocess.TimeoutExpired:p.terminate();code=p.wait(timeout=20)
if (U/'Saved').exists():shutil.copytree(U/'Saved',E/tag,dirs_exist_ok=True)
log=(E/(tag+'.log')).read_text(errors='replace');markers=['CAMPUS_WOODS_REVIEW PASS','FRATERNITY_BOUNDARY_REVIEW PASS','WOODS_REVIEW PASS','HOUSE_VISUAL_REVIEW_PASSED']
if tag!='baseline':markers+=['SURROUNDINGS_POLISH_REVIEW PASS']
passed=code==0 and all(v in log for v in markers)
(E/(tag+'-run.json')).write_text(json.dumps(dict(status='PASS' if passed else 'FAIL',exit_code=code,seconds=time.monotonic()-t,expected_markers=markers),indent=2));print('PASS' if passed else 'FAIL',code,flush=True)
sys.exit(0 if passed else 1)
