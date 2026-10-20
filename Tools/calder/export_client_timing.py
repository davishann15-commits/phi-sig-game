"""Export a completed client trace through headless Unreal Insights.
Run only after the paired game job has exited; never alongside timing tests.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[2]
if len(sys.argv)!=2:
    raise SystemExit('Usage: export_client_timing.py <completed-run-name>')
run=ROOT/'Saved/CalderWork/runs'/sys.argv[1]
trace=run/'client.utrace'
assert trace.is_file() and trace.stat().st_size>0,trace
check=subprocess.run(['pgrep','-f','UnrealEditor.*SeniorSendoff.uproject.*-game'],capture_output=True)
assert check.returncode!=0,'Wait for the paired game processes to exit'
output=run/'game_thread_events.csv'
command='TimingInsights.ExportTimingEvents "'+str(output)+'" -columns=ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Duration,Depth -threads=GameThread -timers=*'
response_folder=tempfile.TemporaryDirectory(prefix='calder_insights_')
response=Path(response_folder.name)/'export.rsp'
response.write_text(command+'\n')
(run/'insights_export.rsp').write_text(command+'\n')
arguments=['/Users/UE_5.8/Engine/Binaries/Mac/UnrealInsights.app/Contents/MacOS/UnrealInsights',
 '-OpenTraceFile='+str(trace),'-ABSLOG='+str(run/'insights_export.log'),'-AutoQuit','-NoUI',
 '-ExecOnAnalysisCompleteCmd=@='+str(response)]
with (run/'insights_export_stdout.log').open('w') as log:
    result=subprocess.run(arguments,cwd=run,stdout=log,stderr=subprocess.STDOUT,timeout=240)
assert result.returncode==0,'Insights failed; inspect export logs'
assert output.exists() and output.stat().st_size>0,'No timing CSV exported'
print('CALDER_CLIENT_TIMING_EXPORT PASS bytes='+str(output.stat().st_size)+' file='+str(output))
