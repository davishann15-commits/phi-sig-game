"""Audit alternating on/off timings as paired observations, preserving raw data.
The original report's difference of separate medians is retained verbatim.
No runs are excluded and no budget changes:paired interior delta<=+/-0.5ms,
original absolute p8p limits,and all actual self-tick p95 values<=0.3ms.
"""
import argparse,csv,hashlib,json,statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];RUNS=ROOT/'Saved/CalderWork/runs'
p=argparse.ArgumentParser();p.add_argument('tag');a=p.parse_args();source=RUNS/(a.tag+'_perf.json');old=json.loads(source.read_text());assert old['complete'] and old['source_frozen']
assert all(hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==sha for name,sha in old['source_sha256'].items()),'Current source differs from captured freeze'
data={}
for mode in ('on','off'):
 for i in (1,2,3):
  f=RUNS/f'{a.tag}_{mode}{i}'/'Saved/HouseExteriorViews/frametimes.csv';data[mode,i]={r['view']:float(r['avg_ms']) for r in csv.DictReader(f.open())};assert set(data[mode,i])==set(old['views'])
views={};interiors=True;absolute=True
for name,v in old['views'].items():
 deltas=[data['on',i][name]-data['off',i][name] for i in (1,2,3)];median=statistics.median(deltas)
 row={'paired_round_deltas_ms':deltas,'median_paired_delta_ms':median,'original_difference_of_separate_medians_ms':v['delta_ms'],'all_three_pairs_retained':True}
 if name.startswith('detail_perf_in_'):row['within_original_noise_budget']=abs(median)<=.5;interiors &= row['within_original_noise_budget']
 if 'master_p8p_frame_budget_passed' in v:absolute &= v['master_p8p_frame_budget_passed']
 views[name]=row
result={'passed':old['wildlife_tick_all_rounds_all_views_within_0_3ms'] and interiors and absolute,'scope':'Corrected-source deer timing; full roster/global load/hill route/master remain separate','method':'Median of the three signed alternating-pair differences; original independent-median report retained','original_report':str(source.relative_to(ROOT)),'original_unpaired_noise_gate_passed':old['listed_frame_budgets_passed'],'paired_interiors_within_0_5ms':interiors,'original_absolute_frame_budgets_passed':absolute,'all_tick_rounds_below_0_3ms':old['wildlife_tick_all_rounds_all_views_within_0_3ms'],'all_six_runs_retained':True,'source_matches_capture_freeze':True,'no_threshold_changes':True,'views':views,'whole_phase3_complete':False}
out=RUNS/(a.tag+'_paired_audit.json');assert not out.exists();out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'passed':result['passed'],'interior_paired_deltas_ms':{n:round(v['median_paired_delta_ms'],3) for n,v in views.items() if n.startswith('detail_perf_in_')},'original_absolute_frame_budgets_passed':absolute,'all_tick_rounds_below_0_3ms':result['all_tick_rounds_below_0_3ms'],'report':str(out)},indent=2));raise SystemExit(0 if result['passed'] else 1)
