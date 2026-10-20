"""Summarize actual road collision samples without changing review criteria."""
import csv
import json
from pathlib import Path
import sys

source=Path(sys.argv[1])
rows=[]
for row in csv.DictReader(source.open()):
    for name in ('across','x','y','expected','hit','gradePct'):
        row[name]=float(row[name])
    row['walkable']=bool(int(row['walkable']))
    row['error_cm']=abs(row['hit']-row['expected'])
    rows.append(row)
errors=sorted(rows,key=lambda row:row['error_cm'],reverse=True)
grades=sorted(rows,key=lambda row:row['gradePct'],reverse=True)
bad=[row for row in rows if not row['walkable'] or row['error_cm']>1. or row['gradePct']>12.]
components={}
for row in bad:
    components[row['component']]=components.get(row['component'],0)+1
report={'source':str(source),'samples':len(rows),'bad_rows':len(bad),
        'bad_components':components,'worst_height_rows':errors[:12],'worst_grade_rows':grades[:12]}
out=source.with_name('road_profile_diagnosis.json')
out.write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
