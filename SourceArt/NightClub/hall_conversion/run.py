import pathlib,sys,subprocess,shutil,json,time
W=pathlib.Path(__file__).resolve().parent;R=W.parents[2];tag=sys.argv[1];U=pathlib.Path('/tmp/club-hall-'+tag);U.mkdir(exist_ok=True);E=W/'evidence'
assert subprocess.run(['pgrep','-x','UnrealEditor'],capture_output=True).returncode!=0
extra={'walk':['-ClubHallWalkReview'],'pole':['-HouseClubPoleSmoke'],'views':['-HouseVisualReview','-HouseVisualBenchmark','-HouseVisualQuality=2','-HouseVisualLayout='+str(W/'views.json')], 'finalviews':['-HouseVisualReview','-HouseCleanupProbe','-HouseVisualQuality=2','-HouseVisualLayout='+str(W/'views-final.json')]}[tag]
cmd=['/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor',str(R/'SeniorSendoff.uproject'),'/Game/Story/Maps/Chapter01_House','-game','-windowed','-ResX=1280','-ResY=720','-NoSplash','-NoTraceServer','-UserDir='+str(U),'-AbsLog='+str(E/(tag+'.log'))]+extra+['-ExecCmds=t.MaxFPS 0,r.VSync 0']
start=time.monotonic()
with (E/(tag+'-stdout.log')).open('w')as f:
 p=subprocess.Popen(cmd,stdout=f,stderr=subprocess.STDOUT)
 try:code=p.wait(timeout=240)
 except subprocess.TimeoutExpired:p.terminate();code=p.wait(timeout=20)
if (U/'Saved').exists():shutil.copytree(U/'Saved',E/tag,dirs_exist_ok=True)
(E/(tag+'-run.json')).write_text(json.dumps(dict(exit_code=code,seconds=time.monotonic()-start),indent=2));print('Exit',code,flush=True)
