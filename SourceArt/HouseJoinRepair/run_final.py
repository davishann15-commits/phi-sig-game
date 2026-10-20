import pathlib,subprocess,sys,json,shutil
B=pathlib.Path(__file__).resolve().parent
for stage,args in [('final',['run_views.py','final']),('details',['run_views.py','details','details.json','3']),('windows',['run_check.py','HouseConnectionsReview','windows']),('frontdoors',['run_check.py','FrontDoubleDoorReview','frontdoors'])]:
 print('START '+stage,flush=True);subprocess.run([sys.executable,str(B/args[0]),*args[1:]],check=True)
 if stage in['final','details']:
  r=json.loads((B/'evidence'/stage/'review.json').read_text());assert r['status']=='PASS',r
 elif stage=='windows':
  N=B.parent/'HouseExteriorConnections/evidence/native';text=(N/'result.txt').read_text();shutil.copytree(N,B/'evidence/windows/Connections',dirs_exist_ok=True);assert text.rstrip().endswith('PASS'),text[-1000:]
 else:
  r=(B/'evidence/frontdoors/FrontDoubleDoorReview/result.txt').read_text();assert 'PASS Rendered front doors' in r,r[-1000:]
 print('PASS '+stage,flush=True)
