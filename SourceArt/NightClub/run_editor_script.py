import subprocess,pathlib,sys,time,json
R=pathlib.Path(__file__).resolve().parents[2];B=R/'SourceArt/NightClub';name=sys.argv[1];log=B/'evidence'/(name+'.log')
p=subprocess.Popen(['/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor',str(R/'SeniorSendoff.uproject'),'-ExecutePythonScript='+str(B/(name+'.py')),'-NoSplash','-AbsLog='+str(log)],stdout=open(B/'evidence'/(name+'-stdout.log'),'w'),stderr=subprocess.STDOUT)
print('Started scoped Unreal editor script',name,flush=True)
try:code=p.wait(timeout=300)
except subprocess.TimeoutExpired:p.terminate();code=p.wait(timeout=20)
print('Exit',code,'log',log,flush=True)
if code:sys.exit(code)
