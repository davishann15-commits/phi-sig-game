"""Serialized baseline capture/reviews. Only camera instrumentation changes Source; never saves Content."""
from pathlib import Path
from datetime import datetime,timezone
import json,subprocess,hashlib,sys,re,csv,time
R=Path(__file__).resolve().parents[2];W=R/'Saved/FraternityRowWork'
if '--wait-for-perf' in sys.argv:
    while not (W/'baseline_perf_receipt.json').exists():time.sleep(5)
assert json.loads((W/'baseline_perf_receipt.json').read_text())['passed']
assert not (W/'before_checks_receipt.json').exists(),'Before receipt already exists; inspect instead of overwriting'
receipt=json.loads((W/'baseline_source_receipt.json').read_text())
for rel,sha in receipt['sha256'].items():
    assert hashlib.sha256((R/rel).read_bytes()).hexdigest()==sha,'Source changed before camera instrumentation: '+rel
with (W/'before_checks.log').open('w') as stream:
    def command(args):
        stream.write('\nSTART '+repr(args)+'\n');stream.flush()
        p=subprocess.run(args,cwd=R,stdout=stream,stderr=subprocess.STDOUT)
        stream.flush();return p.returncode
    assert command(['python3','SourceArt/FraternityRow/instrument_views.py'])==0
    assert command(['bash','Saved/CalderWork/tools/build.sh'])==0,'Before-camera build failed'
    (W/'camera_build.log').write_bytes((R/'Saved/CalderWork/build.log').read_bytes())
    G='Saved/CalderWork/tools/run_game.sh';runs=[]
    def game(suffix,seconds,*args):
        tag='row_current_20261018_'+suffix;out=R/'Saved/CalderWork/runs'/tag
        assert not out.exists(),'Unique run already exists: '+tag
        rc=command(['bash',G,tag,str(seconds),'-RenderOffScreen','-nosound',*args])
        log=out/'Saved/Logs/SeniorSendoff.log';text=log.read_text(errors='replace') if log.exists() else ''
        verdicts=[x for x in text.splitlines() if re.search(r'\b(PASS|FAIL|[A-Z_]+_PASSED|[A-Z_]+_FAILED)\b',x) and ('LogTemp' in x or 'Fatal' in x)]
        runs.append({'tag':tag,'wrapper_exit':rc,'verdicts':verdicts,'saved':str(out/'Saved')})
        (W/'before_checks_progress.json').write_text(json.dumps(runs,indent=2)+'\n')
    static=['-CampusLayoutReview','-CampusWoodsReview','-FraternityBoundaryReview','-SurroundingsPolishReview','-ParkingLotReview','-WoodsPassReview','-WestFieldReview','-WoodsSceneReview']
    game('dusk',600,'-HouseExteriorViews','-CleanViews','-ExteriorViewFilter=detail_row_','-ExteriorViewWarmup=2','-ExteriorViewSamples=60','-CalderNightfallSeconds=0',*static)
    game('dark',600,'-HouseExteriorViews','-CleanViews','-ExteriorViewFilter=detail_row_','-ExteriorViewWarmup=2','-ExteriorViewSamples=60','-CalderNightfallSeconds=1800')
    for key,seconds,args in [
        ('terrain',900,['-CalderTerrainReview']),('roam',1800,['-FreeRoamWalkReview']),
        ('parkingwalk',600,['-ParkingLotWalkReview']),('woodswalk',600,['-WoodsWalkReview']),
        ('fieldwalk',600,['-WestFieldWalkReview']),('exteriorwalk',600,['-HouseExteriorWalkReview']),
        ('trees',600,['-HouseTreesReview']),('smoke',1800,['-CampusInteriorsSmoke=All']),
        ('intA',1800,['-CampusInteriorsReview=LIB,HAR,SCI,UNI,STA,TWR']),
        ('intBC',1800,['-CampusInteriorsReview=MAIN,HLTH,COM,ASH,HUM,KER,FLD,ART,MUS,ENG,PLT,ALU']),
        ('house',600,['-CombinedHouseSmoke']),('doors',600,['-ExistingHouseDoorSmoke']),
        ('traversal',600,['-HouseTraversalSmoke'])]:game(key,seconds,*args)
    capture_ok=True
    for suffix in ['dusk','dark']:
        folder=R/'Saved/CalderWork/runs'/('row_current_20261018_'+suffix)/'Saved/HouseExteriorViews'
        frames=list(csv.DictReader((folder/'frametimes.csv').open())) if (folder/'frametimes.csv').exists() else []
        capture_ok &= len(frames)==14 and len(list(folder.glob('*detail_row_*.png')))==14
    result={'finished_utc':datetime.now(timezone.utc).isoformat(),'cameras_complete':capture_ok,'runs':runs}
    (W/'before_checks_receipt.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'before_checks_complete':True,'cameras_complete':capture_ok,'runs':len(runs)}))
