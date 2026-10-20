"""Compare actual original/trial component poses after independent native checks."""
import csv,hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
if len(sys.argv)!=3: raise SystemExit('Usage: compare_tree_fit.py <original-run> <trial-run>')
paths=[ROOT/'Saved/CalderWork/runs'/name for name in sys.argv[1:]]
rows=[]
for expected,path in enumerate(paths):
    folder=path/'Saved/CalderTreeFitReview'
    result=(folder/'result.txt').read_text()
    assert result.startswith('PASS ') and ('trial='+str(expected)+' ') in result,result
    poses=(folder/'poses.csv').read_bytes()
    parsed=list(csv.DictReader(poses.decode().splitlines()))
    assert len(parsed)==65 and len({(r['batch'],r['index']) for r in parsed})==65
    contacts=list(csv.DictReader((folder/'contacts.csv').open()))
    assert len(contacts)==780 and all(r['hit']=='1' and r['item']==r['index'] and float(r['localErrorCm'])<=.5 for r in contacts)
    log=(path/'Saved/Logs/SeniorSendoff.log').read_text()
    hashes=re.findall(r'WOODS_PASS_ORIGINAL_TREES .*?crc=([0-9a-f]+)',log)
    assert hashes and set(hashes)=={'0f3b2bb7'},hashes
    assert 'WOODS_PASS_REVIEW PASS' in log and 'CAMPUS_WOODS_REVIEW PASS' in log
    rows.append({'run':path.name,'pose_sha256':hashlib.sha256(poses).hexdigest(),'poses':poses,'max_contact_error_cm':max(float(r['localErrorCm']) for r in contacts),'native_result':result.strip()})
assert rows[0]['poses']==rows[1]['poses'],'Actual instance transforms changed'
for r in rows: del r['poses']
report={'passed':True,'scope':'Exact actual65component poses and780native trunk-contact queries per run; visual/performance acceptance separate','runs':rows,'original_tree_crc':'0f3b2bb7','all_poses_byte_identical':True}
out=paths[1]/'fit_comparison.json';out.write_text(json.dumps(report,indent=2)+'\n')
print('CALDER_TREE_FIT_COMPARISON PASS poses=65 exactBytes=1 contactsPerRun=780 treeCRC=0f3b2bb7 file='+str(out))
