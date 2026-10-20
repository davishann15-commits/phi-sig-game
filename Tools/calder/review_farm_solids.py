"""Current-source farm collision fallback/server gates; existing reviews unchanged."""
from pathlib import Path
import json
import re
import subprocess
ROOT=Path(__file__).resolve().parents[2]
FLAGS=['-CalderWoodsTreeTrial','-CalderEvergreenAtlasTrial','-CalderRidgeForestTrial','-CalderFallsJetsTrial','-CalderFarmTrial','-CalderRockStrataTrial','-CalderFarmTreeTrial','-CalderHedgerowTrial','-CalderFarmSolidTrial']
REVIEWS=['-CalderFarmSolidReview','-CalderHedgerowReview','-CalderLandscapeReview','-CalderTerrainReview','-WoodsPassReview','-CampusWoodsReview']
CASES=[
 ('off',[x for x in FLAGS if x!='-CalderFarmSolidTrial']+REVIEWS,['CALDER_FARM_SOLID_REVIEW PASS enabled=0','instances=57704','allCalderISMs=24'],['CALDER_FARM_SOLID built']),
 ('noterrain',FLAGS+['-NoCalderTerrain','-CampusLayoutReview','-WoodsPassReview','-CampusWoodsReview','-CalderLandscapeReview','-CalderHedgerowReview','-CalderFarmSolidReview'],['grounds=69240','instances=53740','campusFrontVertices=1091586','floorSamples=2654','allCalderISMs=0','CALDER_FARM_SOLID_REVIEW PASS enabled=0'],['CALDER_FARM_SOLID built','CALDER_PERSIMMON_TRUNK']),
 ('nolandscape',FLAGS+REVIEWS+['-NoCalderLandscape'],['CALDER_TERRAIN_REVIEW PASS terrain=1 hash=72564759','enabled=0 batches=0 instances=0','allCalderISMs=3','CALDER_FARM_SOLID_REVIEW PASS enabled=0'],['CALDER_FARM_SOLID built','CALDER_PERSIMMON_TRUNK']),
 ('nofarm',[x for x in FLAGS if x!='-CalderFarmTrial']+REVIEWS,['instances=57815','allCalderISMs=19','CALDER_FARM_SOLID_REVIEW PASS enabled=0'],['CALDER_FARM_SOLID built','CALDER_FARM_BATCH']),
 ('server',FLAGS+['-server','-NullRHI','-CalderFarmSolidReview','-CalderHedgerowReview','-CalderLandscapeReview'],['CALDER_FARM_SOLID_REVIEW PASS enabled=1 dedicated=1 parts=3','CALDER_PERSIMMON_TRUNK PASS samples=96 hits=96','CALDER_HEDGEROW_REVIEW PASS enabled=0','enabled=0 batches=0 instances=0'],['CALDER_FARM_BATCH','CALDER_HEDGEROW PASS']),
]
def main():
    native=ROOT/'Saved/CalderWork/runs/codex_farm_solid_slope_native/Saved/CalderFarmSolidReview/result.txt'
    reference=native.read_text();assert 'CALDER_FARM_SOLID_REVIEW PASS enabled=1 dedicated=0' in reference
    crc=re.search(r'crc=([0-9a-f]+)',reference).group(1)
    results={}
    for suffix,flags,expected,forbidden in CASES:
        name='codex_farm_solid_final_'+suffix;run=ROOT/'Saved/CalderWork/runs'/name;assert not run.exists(),name
        print('START',name,flush=True)
        subprocess.run(['bash',str(ROOT/'Saved/CalderWork/tools/run_game.sh'),name,'210','-RenderOffScreen','-nosound',*flags],cwd=ROOT,check=True)
        log=(run/'Saved/Logs/SeniorSendoff.log').read_text(errors='replace')
        assert 'CALDER_LANDSCAPE_REVIEW PASS' in log,name
        assert not re.search(r'CALDER_\w+_REVIEW FAIL|CALDER_PERSIMMON_TRUNK FAIL|Fatal error:',log),name
        for marker in expected:assert marker in log,(name,marker)
        for marker in forbidden:assert marker not in log,(name,marker)
        if suffix=='server':assert 'crc='+crc in (run/'Saved/CalderFarmSolidReview/result.txt').read_text()
        results[name]={'passed':True,'expected':expected,'forbidden_absent':forbidden,'log':str((run/'Saved/Logs/SeniorSendoff.log').relative_to(ROOT))}
        if suffix=='server':results[name]['collider_crc_matches_client']=crc
        out=ROOT/'Saved/CalderWork/farm_solid_trial/final_gates.json';out.unlink(missing_ok=True);out.write_text(json.dumps(results,indent=2)+'\n')
        print('PASS',name,flush=True)
    print('ALL FIVE FARM-SOLID GATES PASS',flush=True)
if __name__=='__main__':main()
