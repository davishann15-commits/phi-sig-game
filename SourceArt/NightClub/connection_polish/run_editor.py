"""Run a sequential Unreal editor Python stage, never alongside another editor."""
import pathlib,subprocess,sys,time,json
B=pathlib.Path(__file__).resolve().parent;R=B.parents[2];stage=sys.argv[1]
assert stage in ['export_context','install','fix_tabletops','finish_floor_mapping'],stage
if subprocess.run(['pgrep','-x','UnrealEditor'],capture_output=True).returncode==0:raise SystemExit('Close the saved editor before this stage.')
E=B/'evidence';E.mkdir(exist_ok=True);log=E/(stage+'.log')
cmd=['/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor',str(R/'SeniorSendoff.uproject'),'/Game/Story/Maps/Chapter01_House','-ExecutePythonScript='+str(B/(stage+'.py')),'-NoSplash','-AbsLog='+str(log)]
start=time.monotonic()
with (E/(stage+'-stdout.log')).open('w') as out:
 p=subprocess.Popen(cmd,stdout=out,stderr=subprocess.STDOUT)
 try:code=p.wait(timeout=420)
 except subprocess.TimeoutExpired:
  p.terminate();code=p.wait(timeout=20)
txt=log.read_text(errors='replace') if log.exists() else ''
marker={'export_context':'CLUB_POLISH_CONTEXT_PASS','install':'CLUB_POLISH_INSTALL_PASS','fix_tabletops':'CLUB_POLISH_TABLETOPS_PASS','finish_floor_mapping':'CLUB_POLISH_FLOOR_MAPPING_PASS'}[stage]
status='PASS' if code==0 and marker in txt and 'Failed to compile Material' not in txt else 'FAIL'
report=dict(status=status,stage=stage,seconds=round(time.monotonic()-start,2),exit_code=code,log=str(log))
(E/(stage+'-run.json')).write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2),flush=True)
raise SystemExit(0 if status=='PASS' else 1)
