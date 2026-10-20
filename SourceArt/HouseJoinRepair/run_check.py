import pathlib,sys,subprocess,shutil,json,time
B=pathlib.Path(__file__).resolve().parent;R=B.parents[1];flag=sys.argv[1];tag=sys.argv[2]if len(sys.argv)>2 else flag;E=B/'evidence';U=pathlib.Path('/tmp/house-joins-check-'+tag);U.mkdir(exist_ok=True)
assert subprocess.run(['pgrep','-x','UnrealEditor'],capture_output=True).returncode!=0
cmd=['/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor',str(R/'SeniorSendoff.uproject'),'/Game/Story/Maps/Chapter01_House','-game','-windowed','-ResX=1280','-ResY=720','-NoSplash','-NoTraceServer','-UserDir='+str(U),'-AbsLog='+str(E/(tag+'.log')),'-'+flag,'-ExecCmds=sg.ViewDistanceQuality 2,sg.AntiAliasingQuality 2,sg.ShadowQuality 2,sg.GlobalIlluminationQuality 2,sg.ReflectionQuality 2,sg.PostProcessQuality 2,t.MaxFPS 0,r.VSync 0']
st=time.monotonic()
with (E/(tag+'-stdout.log')).open('w')as f:
 p=subprocess.Popen(cmd,stdout=f,stderr=subprocess.STDOUT)
 try:code=p.wait(timeout=330)
 except subprocess.TimeoutExpired:p.terminate();code=p.wait(timeout=20)
if (U/'Saved').exists():shutil.copytree(U/'Saved',E/tag,dirs_exist_ok=True)
(E/(tag+'-run.json')).write_text(json.dumps(dict(exit_code=code,seconds=time.monotonic()-st),indent=2));print(code)
