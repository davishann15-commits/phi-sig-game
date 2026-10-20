"""Deer-first timing: original300-frame/4s warmup method,3alternating pairs.
Includes sound and all campus interiors. Original renderer/geometry remain
common to both arms; NoCalderWildlife restores original cricket behavior.
Noise deltas use the median of the three alternating paired differences;
independent on/off medians and every paired difference are also retained.
Writes no map or assets, never overwrites a named run, freezes Source/Config.
This does not accept unfinished species or the original global hill-route gate.
"""
import argparse,csv,hashlib,json,math,re,statistics,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];RUNS=ROOT/'Saved/CalderWork/runs'
BASE={'front_street','street_along','detail_establishing_corner','detail_street_dusk','detail_clocktower_far','detail_campus_quad','detail_layout_quad','detail_layout_gateway','detail_stadium_gate','detail_layout_aerial','detail_layout_dormrow','detail_perf_in_lib','detail_perf_in_harlow','detail_perf_in_science','detail_perf_in_union','detail_perf_in_stadium'}
WOODS={'detail_woods_edge','detail_woods_20m','detail_woods_deep','detail_woods_ravine','detail_woods_silhouette'}
VIEWS=BASE|WOODS
P8P_LIMITS={'front_street':22.96,'street_along':15.15,'detail_layout_quad':9.96,'detail_campus_quad':9.58,'detail_layout_aerial':16.07}
INTERIORS={v for v in VIEWS if v.startswith('detail_perf_in_')}

def source_hashes():
    return {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for folder in ('Source','Config') for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix in ('.h','.cpp','.inl','.cs','.ini')}

def csv_ticks(directory,view_names):
    profiles=sorted((directory/'Saved/Profiling/CSV').glob('*.csv'),key=lambda p:p.stat().st_mtime_ns)
    if len(profiles)!=len(view_names):raise RuntimeError(f'CSV count {len(profiles)} differs from view count {len(view_names)} in {directory}')
    results={}
    for name,path in zip(view_names,profiles):
        lines=list(csv.reader(path.open()));header=next((row for row in lines if any('CalderWildlife/TickMs' in item for item in row)),None)
        if header is None:raise RuntimeError(f'No actual wildlife self-time column: {path}')
        col=next(i for i,item in enumerate(header) if 'CalderWildlife/TickMs' in item);samples=[]
        start=lines.index(header)+1
        for row in lines[start:]:
            try:value=float(row[col])
            except (ValueError,IndexError):continue
            if math.isfinite(value) and value>0:samples.append(value)
        if len(samples)<290:raise RuntimeError(f'Insufficient actual tick samples for {name}: {len(samples)}')
        samples.sort();results[name]={'samples':len(samples),'p50_ms':statistics.median(samples),'p95_ms':samples[min(len(samples)-1,int(len(samples)*.95))],'file':str(path.relative_to(ROOT))}
    return results

def report(tag,freeze):
    rows={};ticks={};order=None
    for mode in ('on','off'):
        rows[mode]={v:[] for v in VIEWS};ticks[mode]=[]
        for round_number in (1,2,3):
            folder=RUNS/f'{tag}_{mode}{round_number}';path=folder/'Saved/HouseExteriorViews/frametimes.csv';data=list(csv.DictReader(path.open()));names=[r['view'] for r in data]
            assert set(names)==VIEWS and len(names)==len(set(names)),(path,set(names)^VIEWS)
            if order is None:order=names
            assert names==order
            log=(folder/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace');assert 'HOUSE_EXTERIOR_VIEWS done' in log and 'Fatal error:' not in log
            if mode=='on':
                assert 'customFloats=253' in log and 'CALDER_WILDLIFE_SOUND_SUBMIX_REGISTERED' in log
                ticks[mode].append(csv_ticks(folder,names))
            else:assert 'CALDER_WILDLIFE_ROSTER' not in log and 'CALDER_WILDLIFE_SOUND_READY' not in log
            for row in data:
                values={k:float(row[k]) for k in ('avg_ms','game_ms','render_ms')};assert all(math.isfinite(x) and x>=0 for x in values.values());rows[mode][row['view']].append(values)
    summary={};tick_ok=True;frames_ok=True
    for view in order:
        entry={mode:{key:statistics.median(row[key] for row in rows[mode][view]) for key in ('avg_ms','game_ms','render_ms')} for mode in ('on','off')}
        entry['independent_median_delta_ms']=entry['on']['avg_ms']-entry['off']['avg_ms']
        entry['paired_round_deltas_ms']=[rows['on'][view][i]['avg_ms']-rows['off'][view][i]['avg_ms'] for i in range(3)]
        entry['delta_ms']=statistics.median(entry['paired_round_deltas_ms']);entry['wildlife_tick_rounds']=[r[view] for r in ticks['on']]
        entry['wildlife_tick_p95_median_ms']=statistics.median(r[view]['p95_ms'] for r in ticks['on']);entry['wildlife_tick_budget_passed']=all(r[view]['p95_ms']<=.3 for r in ticks['on']);tick_ok &= entry['wildlife_tick_budget_passed']
        if view in P8P_LIMITS:
            entry['master_p8p_limit_ms']=P8P_LIMITS[view];entry['master_p8p_frame_budget_passed']=entry['on']['avg_ms']<=P8P_LIMITS[view];frames_ok &= entry['master_p8p_frame_budget_passed']
        if view in INTERIORS:
            entry['interior_delta_within_noise']=abs(entry['delta_ms'])<=.5;frames_ok &= entry['interior_delta_within_noise']
        summary[view]=entry
    assert source_hashes()==freeze,'Source/config changed during performance comparisons'
    result={'complete':True,'scope':'Deer-first on/off; full species/global master/hill-route acceptance pending','source_frozen':True,'source_sha256':freeze,'editor_closed_at_start':True,'rounds':3,'alternating':True,'views':summary,'wildlife_tick_all_rounds_all_views_within_0_3ms':tick_ok,'listed_frame_budgets_passed':frames_ok,'global_load_and_remaining_species_acceptance_pending':True,'scene_time':300,'samples_per_view':300,'warmup_seconds':4,'all_campus_interiors':True,'sound_enabled':True,'whole_phase3_complete':False}
    out=RUNS/(tag+'_perf.json');assert not out.exists();out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'complete':True,'views':len(summary),'tick_budget_passed':tick_ok,'report':str(out)}),flush=True)
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('tag');p.add_argument('--breakdown',action='store_true');args=p.parse_args();assert re.fullmatch('[a-z0-9_]+',args.tag)
    inventory=subprocess.check_output(['ps','-axo','comm='],text=True).splitlines()
    assert not any(('/UnrealEditor' in item and 'UnrealEditorServices' not in item) or item.endswith('/SeniorSendoff') for item in inventory),'Unreal/game must be closed before isolated timings'
    for i in (1,2,3):
        for mode in ('on','off'):assert not (RUNS/f'{args.tag}_{mode}{i}').exists()
    freeze=source_hashes();freeze_path=RUNS/(args.tag+'_source_freeze.json');assert not freeze_path.exists();freeze_path.write_text(json.dumps(freeze,indent=2)+'\n')
    flags=['-RenderOffScreen','-MetalOffscreenOnly','-muteaudio','-NoTraceServer','-HouseExteriorViews','-CleanViews','-CampusInteriorsAll','-CalderSceneTime=300','-CalderNightfallSeconds=0','-ExteriorViewSamples=300','-ExteriorViewWarmup=4','-ExteriorViewCsv','-ExteriorViewFilter='+','.join(sorted(VIEWS)),'-ExecCmds=t.MaxFPS 0,r.VSync 0,r.InstanceCulling.OcclusionCull 1,r.GPUCsvStatsEnabled 1']
    if args.breakdown:flags.append('-CalderWildlifePerfBreakdown')
    for i in (1,2,3):
        for mode in ('on','off'):
            name=f'{args.tag}_{mode}{i}';log=RUNS/(name+'_launcher.log');assert not log.exists()
            with log.open('w') as stream:subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),name,'900',*flags,*(['-NoCalderWildlife'] if mode=='off' else [])],cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT,check=True)
            assert source_hashes()==freeze,'Source/config changed during performance capture';print('WILDLIFE_PERF completed '+name,flush=True)
    result=report(args.tag,freeze);raise SystemExit(0 if result['wildlife_tick_all_rounds_all_views_within_0_3ms'] and result['listed_frame_budgets_passed'] else 1)
