"""Read actual render-thread CSVs; never substitute helper frame-time deltas for render cost."""
from pathlib import Path
import csv,json,statistics,sys
R=Path(__file__).resolve().parents[2];runs=R/'Saved/CalderWork/runs';O=R/'SourceArt/FraternityRow'
before='row_before_20261018_r1';after=sys.argv[1] if len(sys.argv)>1 else None;lights=sys.argv[2] if len(sys.argv)>2 else None
assert not after or lights, 'Final report requires the separate six-light A/B'
views=['front_street','street_along','detail_establishing_corner','detail_street_dusk','detail_clocktower_far','detail_stadium_gate','detail_campus_quad','detail_layout_gateway','detail_layout_quad','detail_layout_aerial','detail_layout_dormrow']
def load(tag,mode):
    data={v:[] for v in views};receipt=[]
    for i in range(1,4):
        p=runs/f'{tag}_{mode}{i}'/'Saved/HouseExteriorViews/frametimes.csv'
        assert p.exists(),'Incomplete performance run: '+str(p)
        rows=list(csv.DictReader(p.open()));assert len(rows)==11 and set(x['view'] for x in rows)==set(views),str(p)
        for row in rows:data[row['view']].append(float(row['render_ms']))
        receipt.append(str(p))
    return data,receipt
b1,bp1=load(before,'on');b2,bp2=load(before,'off');b={v:b1[v]+b2[v] for v in views};receipt=bp1+bp2
modes={}
if after:
    for m in ['on','off']:modes[m],paths=load(after,m);receipt+=paths
if lights:
    for m in ['on','off']:modes['lights_'+m],paths=load(lights,m);receipt+=paths
rows=[]
for v in views:
    base=statistics.median(b[v]);row={'view':v,'baseline_ms':base,'baseline_min':min(b[v]),'baseline_max':max(b[v])}
    for mode,values in modes.items():row[mode+'_ms']=statistics.median(values[v])
    if after:
        row['before_to_after_pct']=100*(row['on_ms']/base-1)
        row['upgrade_ab_pct']=100*(row['on_ms']/row['off_ms']-1)
    if lights:row['six_lights_ab_pct']=100*(row['lights_on_ms']/row['lights_off_ms']-1)
    rows.append(row)
result={'metric':'render_ms','sample_unit':'per-run mean, median of three alternating rounds; baseline median of six unchanged observations','rows':rows,'csv_files':receipt,'passes_before_budget':all(x['before_to_after_pct']<=5 for x in rows) if after else None,'passes_upgrade_ab_budget':all(x['upgrade_ab_pct']<=5 for x in rows) if after else None}
(O/'PERFORMANCE.json').write_text(json.dumps(result,indent=2)+'\n')
lines=['# Fraternity Row render-thread measurements','',result['sample_unit']+'. The unchanged baseline range is included to expose measurement variability.','', '| Standard view | Before median ms | Before range ms | After median ms | Before/after | Upgrade A/B | Six lights A/B |','|---|---:|---:|---:|---:|---:|---:|']
for x in rows:
    number=lambda k:f"{x[k]:.2f}" if k in x else 'pending'
    percent=lambda k:f"{x[k]:+.1f}%" if k in x else 'pending'
    lines.append(f"| {x['view']} | {x['baseline_ms']:.2f} | {x['baseline_min']:.2f}–{x['baseline_max']:.2f} | {number('on_ms')} | {percent('before_to_after_pct')} | {percent('upgrade_ab_pct')} | {percent('six_lights_ab_pct')} |")
lines+=['','Full upgrade A/B uses `-NoFraternityRowUpgrade`; light-only A/B uses `-NoFraternityRowLights` and retains every fixture/emissive/house/yard. Both compare exactly the same11 standard views. Render-thread budget is no view worse than+5%. No unmeasured result is claimed.']
(O/'PERFORMANCE.md').write_text('\n'.join(lines)+'\n')
print(json.dumps({k:result[k] for k in ['metric','passes_before_budget','passes_upgrade_ab_budget']}))
if after and not (result['passes_before_budget'] and result['passes_upgrade_ab_budget']):
    raise SystemExit(2)
