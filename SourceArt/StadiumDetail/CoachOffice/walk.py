from pathlib import Path
from datetime import datetime,timezone
import sys,json,re,argparse
ROOT=Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT/'SourceArt/StadiumDetail'))
from stage_runner import native,idle,manifest,sha,WORK,RUNS
p=argparse.ArgumentParser();p.add_argument('tag');a=p.parse_args();assert re.fullmatch('[a-z0-9_]+',a.tag)
idle();frozen=manifest()
with (WORK/(a.tag+'_walk.log')).open('x') as output:log=native(a.tag,['-StadiumCoachOfficeReview','-CalderNightfallReview','-CalderNightfallSeconds=0'],output)
idle();assert manifest()==frozen
summary=(RUNS/a.tag/'Saved/StadiumCoachOfficeReview/result.txt').read_text();assert 'PASS reached=8/8 doors=4/4 indoor=3/3 shellRays=4' in summary
assert 'CALDER_STADIUM_COACH_REVIEW PASS' in log
(WORK/(a.tag+'_walk.json')).write_text(json.dumps({'passed':True,'stage':7,'tag':a.tag,'run_tags':[a.tag],'source_config_map_sha':frozen,'utc':datetime.now(timezone.utc).isoformat(),'summary':summary,'final_acceptance':False,'evidence_files':[str((RUNS/a.tag/'Saved/StadiumCoachOfficeReview/result.txt').relative_to(ROOT)),str((RUNS/a.tag/'Saved/Logs/SeniorSendoff.log').relative_to(ROOT))]},indent=2)+'\n');print(summary)
