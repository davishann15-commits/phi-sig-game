"""Original69-entry/65-walking-check STA tour at night; original reviewer stays exact."""
from pathlib import Path
from datetime import datetime,timezone
import argparse,json,re,subprocess
from stage_runner import ROOT,WORK,RUNS,idle,manifest,sha

parser=argparse.ArgumentParser()
parser.add_argument('tag')
parser.add_argument('--stage',type=int,choices=(6,7),required=True)
args=parser.parse_args()
assert re.fullmatch(r'[a-z0-9_]+',args.tag)
idle();frozen=manifest()
grant=json.loads((WORK/'integration_grant.json').read_text());assert grant['granted'] and grant['scope']=='full' and grant['release_evidence']
assert not (RUNS/args.tag).exists()
receipt=WORK/(args.tag+'_night_stadium.json');assert not receipt.exists()
with (WORK/(args.tag+'_night_stadium.log')).open('x') as output:
 command=['bash','Saved/CalderWork/tools/run_game.sh',args.tag,'1800','-RenderOffScreen','-MetalOffscreenOnly','-nosound','-CampusInteriorsReview=STA','-CalderNightfallReview','-CalderNightfallSeconds=1800','-StadiumDetailReview']
 output.write(repr(command)+'\n');output.flush();subprocess.run(command,cwd=ROOT,stdout=output,stderr=subprocess.STDOUT,check=True)
idle();assert manifest()==frozen
folder=RUNS/args.tag;log=(folder/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');summary=(folder/'Saved/CampusInteriorsReview/result.txt').read_text()
assert not re.search(r'REVIEW FAIL|Fatal error|Assertion failed',log)
assert f'CALDER_STADIUM built step={args.stage} ' in log
assert 'CALDER_STADIUM_REVIEW PASS enabled=1' in log
assert 'CALDER_STADIUM_WALL_SEAMS PASS rays=42 leaks=0 actualStreamedRoom=1' in log
assert 'CALDER_STADIUM_WALL_SEAMS FAIL' not in log
night_rows=re.findall(r'CALDER_NIGHTFALL seconds=([0-9.]+) alpha=([0-9.]+) duration=1800 pinned=(\d)',log)
assert len(night_rows)>=2 and all(float(t)==1800 and float(a)==1 and pin=='1' for t,a,pin in night_rows),night_rows
assert 'CAMPUS_INTERIORS_REVIEW PASS buildings=STA stops=65/65 problems=0' in log
verdicts=re.findall(r'^PASS\b[^\n]*',summary,re.M)
assert len(verdicts)==1 and verdicts[0].startswith('PASS buildings=STA stops=65/65 problems=0')
assert not re.search(r'^FAIL\b',summary,re.M)
# The unchanged original reviewer counts only EStep::Walk. Keep all69 actions too.
review_source=ROOT/'Source/SeniorSendoff/HouseCampusInteriorsReview.cpp'
snapshot=json.loads((WORK/'pre_integration_snapshot.json').read_text())
assert sha(review_source)==snapshot['source_config_project_map_sha']['Source/SeniorSendoff/HouseCampusInteriorsReview.cpp']
indices=sorted({int(n) for n in re.findall(r'CAMPUS_INTERIORS_REVIEW stop (\d+):',log)})
walks=re.findall(r'CAMPUS_INTERIORS_REVIEW stop (\d+): reached ',log)
assert indices==list(range(69)) and len(walks)==65 and len(set(walks))==65
for required in ('mainDoorsOpened=1/1','lockedDoorsHeld=1','reasonsShown=1','freedAfterLeaving=1/1','maxInteriorLights=6 pool=6'):
 assert required in summary,(required,summary)
expected=['STA_1_Tower.png','STA_2_PressBox.png','STA_3_Gate5.png','STA_4_Concourse.png','STA_5_Locker.png','STA_6_Tunnel.png','STA_7_Vomitory.png']
paths=[folder/'Saved/CampusInteriorsReview'/name for name in expected];assert all(p.exists() and p.stat().st_size>1000 for p in paths)
result={'stage':args.stage,'tag':args.tag,'passed':True,'source_config_map_sha':frozen,'finished_utc':datetime.now(timezone.utc).isoformat(),'nightfall_seconds':1800,'actual_pinned_nightfall_verified':True,'nightfall_log_samples':len(night_rows),'original_route':'CampusInteriorsReview=STA; all69originalactions/65walkingchecks/doors/criteria; external1800timeoutretained','native_walking_checks':65,'native_tour_entries':69,'original_reviewer_sha256':sha(review_source),'native_summary':summary,'images':[{'file':str(p.relative_to(ROOT)),'sha256':sha(p)} for p in paths],'visual_inspection_pending':True,'scope':'Additional night-room proof; does not replace full11phase review or actualmanualimageinspection'}
receipt.write_text(json.dumps(result,indent=2)+'\n');print('PASS native nightSTA; actual7imageinspectionstillrequired:',receipt)
