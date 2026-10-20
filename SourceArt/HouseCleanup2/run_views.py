import pathlib,subprocess,time,sys,json,shutil
B=pathlib.Path(__file__).resolve().parent;R=B.parents[1];tag=sys.argv[1];E=B/'evidence';U=pathlib.Path('/tmp/house-joins-'+tag);U.mkdir(exist_ok=True)
assert subprocess.run(['pgrep','-x','UnrealEditor'],capture_output=True).returncode!=0
cmd=['/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor',str(R/'SeniorSendoff.uproject'),'/Game/Story/Maps/Chapter01_House','-game','-windowed','-ResX=1280','-ResY=720','-NoSplash','-NoTraceServer','-UserDir='+str(U),'-AbsLog='+str(E/(tag+'.log')),'-HouseVisualReview','-HouseCleanupProbe','-HouseVisualLayout='+str(B/(sys.argv[2]if len(sys.argv)>2 else 'views.json')),'-HouseVisualQuality='+ (sys.argv[3]if len(sys.argv)>3 else '2'),'-HouseVisualOpenPorch','-HouseVisualOpenDoorId=SM_OriginalLeaf_Basement_p0_m2_Solid_01','-HouseVisualBreakLobbyWindow',*(['-HouseVisualBenchmark']if len(sys.argv)<3 or 'benchmark' in sys.argv else []),'-ExecCmds=t.MaxFPS 0,r.VSync 0'+(',r.Nanite 0'if 'nonanite' in sys.argv else '')]
with (E/(tag+'-stdout.log')).open('w') as f:
 p=subprocess.Popen(cmd,stdout=f,stderr=subprocess.STDOUT);code=p.wait(timeout=550)
if (U/'Saved/HouseVisualReview').exists():shutil.copytree(U/'Saved/HouseVisualReview',E/tag,dirs_exist_ok=True)
print(json.dumps(dict(exit_code=code,tag=tag)),flush=True)
