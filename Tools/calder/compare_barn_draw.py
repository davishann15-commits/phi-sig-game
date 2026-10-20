"""Compare actual renderer triangle-corner attributes across the optional barn merge.

Native -CalderFarmDump writes the input CSVs. Batch names are deliberately not
part of the record: material identity, geometry, shading and UVs must survive.
"""
import argparse
from collections import Counter
import csv
import hashlib
import json
from pathlib import Path


def corners(path):
    result=Counter()
    with path.open() as source:
        for row in csv.DictReader(source):
            values=tuple(round(float(row[key]),9) for key in
                         ['x','y','z','nx','ny','nz','tx','ty','tz','sign','u','v'])
            # Normalise negative zero without concealing any nonzero delta.
            values=tuple(0.0 if value==0 else value for value in values)
            result[(int(row['lod']),row['material'],row['parent'],*values)]+=1
    return result


def positions(path):
    result=[]
    with path.open() as source:
        for row in csv.DictReader(source):
            if int(row['batch'])>=2:
                result.append(tuple(row[key] for key in ['batch','lod','instance','vertex','x','y','z']))
    return result


def roots(path):
    with path.open() as source:
        return sorted(tuple(row[key] for key in ['batch','instance','x','y','z']) for row in csv.DictReader(source))


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('baseline',type=Path)
    parser.add_argument('trial',type=Path)
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    a=corners(args.baseline/'Saved/CalderFarmDump/architecture_corners.csv')
    b=corners(args.trial/'Saved/CalderFarmDump/architecture_corners.csv')
    mismatch=list((a-b).items())[:3]+list((b-a).items())[:3]
    assert a==b,('Actual architecture attributes changed',mismatch)
    other_a=positions(args.baseline/'Saved/CalderFarmDump/vertices.csv')
    other_b=positions(args.trial/'Saved/CalderFarmDump/vertices.csv')
    assert other_a==other_b,'Other farm geometry changed'
    root_a=roots(args.baseline/'Saved/CalderLandscapeReview/placements.csv')
    root_b=roots(args.trial/'Saved/CalderLandscapeReview/placements.csv')
    assert root_a==root_b,'Cover poses changed'
    encoded=json.dumps(sorted(a.items()),separators=(',',':'),allow_nan=False).encode()
    report={'passed':True,'source':'Actual native render buffers, not source arrays',
            'architecture_triangle_corners':sum(a.values()),'unique_corner_records':len(a),
            'architecture_attributes':['material slot','parent material','world position','world normal',
                                       'world tangent','handedness','UV0'],
            'canonical_attribute_sha256':hashlib.sha256(encoded).hexdigest(),
            'maximum_attribute_delta':0,'other_farm_vertices_exact':len(other_a),
            'cover_poses_exact':len(root_a),'baseline':str(args.baseline),'trial':str(args.trial)}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.unlink(missing_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print('CALDER_BARN_DRAW_COMPARE PASS '+json.dumps(report))


if __name__=='__main__':
    main()
