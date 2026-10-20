"""Three alternating rendered A/B rounds of the explicit maple trial.

Preserves canonical perf_ab.sh's 300 samples, four-second warmup, uncapped
MetalOffscreenOnly method and all original views; adds four woods views.
Diagnostic only: final pinned scene-clock/full-interior chain is still required.
"""
import csv,hashlib,json,math,re,statistics,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'Saved/CalderWork';RUNS=WORK/'runs'
CANON=WORK/'tools/perf_ab.sh';GAME=WORK/'tools/run_game.sh'
if len(sys.argv)!=2 or not re.fullmatch(r'[A-Za-z0-9_-]+',sys.argv[1]):
    raise SystemExit('Usage: perf_tree_trial.py <unique-tag>')
tag=sys.argv[1];canonical=CANON.read_text()
for marker in ['ExteriorViewSamples=300','ExteriorViewWarmup=4','MetalOffscreenOnly','for I in 1 2 3']:
    assert marker in canonical,marker
original_views=re.search(r'^OUT="([^"]+)"',canonical,re.M).group(1).split(',')
views=original_views+['detail_woods_edge','detail_woods_20m','detail_woods_deep','detail_woods_silhouette']
assert len(views)==15 and len(set(views))==15
plan={'scope':'Maple trial A/B diagnostic; no final pinned-clock/interior acceptance','tag':tag,
      'canonical_sha256':hashlib.sha256(CANON.read_bytes()).hexdigest(),
      'game_launcher_sha256':hashlib.sha256(GAME.read_bytes()).hexdigest(),
      'original_views':original_views,'added_woods_views':views[len(original_views):],
      'rounds':3,'samples_requested_per_view':300,'warmup_seconds':4,'final_acceptance':False,'runs':[]}
output=WORK/(tag+'_perf.json')
assert not output.exists(),output
for i in [1,2,3]:
    for config in ['on','off']:
        assert not (RUNS/(tag+'_'+config+str(i))).exists(),'Run name already exists'
common=['-RenderOffScreen','-MetalOffscreenOnly','-nosound','-CalderWoodsTreeTrial','-HouseExteriorViews','-CleanViews',
        '-ExteriorViewSamples=300','-ExteriorViewWarmup=4','-ExteriorViewFilter='+','.join(views),'-ExecCmds=t.MaxFPS 0']
for i in [1,2,3]:
    for config in ['on','off']:
        assert hashlib.sha256(CANON.read_bytes()).hexdigest()==plan['canonical_sha256']
        assert hashlib.sha256(GAME.read_bytes()).hexdigest()==plan['game_launcher_sha256']
        name=tag+'_'+config+str(i);args=['bash',str(GAME),name,'600',*common]
        if config=='off': args.append('-NoCalderTrees')
        print('CALDER_TREE_PERF_START '+name,flush=True)
        with (WORK/(name+'_launcher.log')).open('w') as log:
            subprocess.run(args,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
        path=RUNS/name
        log=(path/'Saved/Logs/SeniorSendoff.log').read_text()
        assert 'HOUSE_EXTERIOR_VIEWS done' in log and 'Assertion failed:' not in log and 'CALDER_WOODS_TREE_FAILED' not in log,name
        active=re.findall(r'CALDER_WOODS_TREE name=',log)
        assert len(active)==(2 if config=='on' else 0),(name,len(active))
        with (path/'Saved/HouseExteriorViews/frametimes.csv').open() as f: rows=list(csv.DictReader(f))
        assert len(rows)==len(views) and {r['view'] for r in rows}==set(views),(name,[r['view'] for r in rows])
        for row in rows:
            for key in ['avg_ms','game_ms','render_ms','max_ms']:
                assert math.isfinite(float(row[key])) and float(row[key])>0,(name,row)
        plan['runs'].append({'name':name,'config':config,'round':i,'rows':rows,'arguments':args})
        output.write_text(json.dumps(plan,indent=2)+'\n')
        print('CALDER_TREE_PERF_DONE '+name+' views='+str(len(rows)),flush=True)
summary=[]
for view in views:
    result={'view':view}
    for config in ['on','off']:
        rows=[row for run in plan['runs'] if run['config']==config for row in run['rows'] if row['view']==view]
        assert len(rows)==3,(view,config)
        result[config]={key:statistics.median(float(r[key]) for r in rows) for key in ['avg_ms','game_ms','render_ms','max_ms']}
    result['delta_ms']=result['on']['avg_ms']-result['off']['avg_ms'];summary.append(result)
plan['complete']=True;plan['summary']=summary;output.write_text(json.dumps(plan,indent=2)+'\n')
print('CALDER_TREE_PERF_COMPLETE '+str(output),flush=True)
for row in summary:print(row['view']+' on=%.2f off=%.2f delta=%+.2fms'%(row['on']['avg_ms'],row['off']['avg_ms'],row['delta_ms']),flush=True)
