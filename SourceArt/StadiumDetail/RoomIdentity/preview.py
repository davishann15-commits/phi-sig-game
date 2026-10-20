from pathlib import Path
from datetime import datetime,timezone
import json,sys,re,argparse
ROOT=Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT/'SourceArt/StadiumDetail'))
from stage_runner import native,idle,manifest,sha,WORK,RUNS
p=argparse.ArgumentParser();p.add_argument('tag');a=p.parse_args();assert re.fullmatch('[a-z0-9_]+',a.tag)
idle();frozen=manifest();g=json.loads((WORK/'integration_grant.json').read_text());assert g['granted'] and g['scope']=='full'
names=['detail_stadium_locker','detail_stadium_tunnel','detail_stadium_trainer','detail_stadium_whirlpool','detail_stadium_wood_lockers']
with (WORK/(a.tag+'_proof.log')).open('x') as stream:log=native(a.tag,['-HouseExteriorViews','-CleanViews','-ExteriorViewFilter='+','.join(names),'-ExteriorViewSamples=60','-ExteriorViewWarmup=4','-StadiumDetailReview','-CalderNightfallReview','-CalderNightfallSeconds=0','-ExecCmds=t.MaxFPS 0'],stream)
idle();assert manifest()==frozen
assert 'CALDER_STADIUM_REVIEW PASS enabled=1 step=7' in log and 'CALDER_STADIUM_WALL_SEAMS PASS rays=42 leaks=0 actualStreamedRoom=1' in log
images=sorted((RUNS/a.tag/'Saved/HouseExteriorViews').glob('*.png'));assert len(images)==len(names)
result={'tag':a.tag,'stage':7,'native_passed':True,'final_acceptance':False,'all_images_inspected':False,'source_config_map_sha':frozen,'utc':datetime.now(timezone.utc).isoformat(),'images':[{'file':str(x.relative_to(ROOT)),'sha256':sha(x)} for x in images],'scope':'Owner room-identity corner proof only; all final stage7 gates remain. Exact prior five camera poses; no route/view changes.'}
(WORK/(a.tag+'_proof.json')).write_text(json.dumps(result,indent=2)+'\n');print('Native five-view identity proof finished; actual inspection remains.')
