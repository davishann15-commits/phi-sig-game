import pathlib,subprocess,sys,time,json
B=pathlib.Path(__file__).resolve().parent;R=B.parents[1];stage=sys.argv[1];E=B/'evidence'
if subprocess.run(['pgrep','-x','UnrealEditor'],capture_output=True).returncode==0:raise SystemExit('Another Unreal process is open.')
cmd=['/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor',str(R/'SeniorSendoff.uproject'),'/Game/Story/Maps/Chapter01_House','-ExecutePythonScript='+str(B/(stage+'.py')),'-NoSplash','-NoTraceServer','-AbsLog='+str(E/(stage+'.log'))]
st=time.monotonic()
with (E/(stage+'-stdout.log')).open('w') as out:
 p=subprocess.Popen(cmd,stdout=out,stderr=subprocess.STDOUT)
 try:code=p.wait(timeout=550)
 except subprocess.TimeoutExpired:p.terminate();code=p.wait(timeout=20)
r=dict(stage=stage,exit_code=code,seconds=time.monotonic()-st);(E/(stage+'-run.json')).write_text(json.dumps(r,indent=2));print(json.dumps(r),flush=True)
raise SystemExit(code)
