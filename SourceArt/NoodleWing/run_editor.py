import pathlib,subprocess,sys,time,json
B=pathlib.Path(__file__).resolve().parent;R=B.parents[1];stage=sys.argv[1];script=B/(stage+'.py')
assert script.is_file()
assert subprocess.run(['pgrep','-x','UnrealEditor'],capture_output=True).returncode!=0,'Close the saved editor first.'
E=B/'evidence';E.mkdir(exist_ok=True);log=E/(stage+'.log');t=time.monotonic()
with (E/(stage+'-stdout.log')).open('w') as out:
 p=subprocess.Popen(['/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor',str(R/'SeniorSendoff.uproject'),'/Game/Story/Maps/Chapter01_House','-ExecutePythonScript='+str(script),'-NoSplash','-AbsLog='+str(log)],stdout=out,stderr=subprocess.STDOUT)
 try:code=p.wait(timeout=700)
 except subprocess.TimeoutExpired:p.terminate();code=p.wait(timeout=20)
txt=log.read_text(errors='replace')
report=dict(stage=stage,code=code,seconds=round(time.monotonic()-t,2),markers=[l for l in txt.splitlines() if 'NOODLE_' in l or 'Traceback' in l or 'Failed to compile Material' in l])
(E/(stage+'-run.json')).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
