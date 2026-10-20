from pathlib import Path
import json
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).parent
paths={name:ROOT/'Source/SeniorSendoff'/name for name in ['HouseStadiumDetailMood.cpp','HouseStadiumDetailLook.cpp','HouseCampusFall.cpp']}
for name,path in paths.items():assert path.read_bytes()==(HERE/(name+'.before')).read_bytes(),name
header=ROOT/'Source/SeniorSendoff/HouseStadiumDetail.h';(HERE/'HouseStadiumDetail.h.before').write_bytes(header.read_bytes())
s=paths['HouseStadiumDetailMood.cpp'].read_text();s=s.replace('#include "Misc/Paths.h"','#include "Misc/Paths.h"\n#include "Misc/Base64.h"\n#include "Misc/Compression.h"',1);anchor='USoundSubmix* AHouseStadiumDetail::RegisteredSoundReviewMix';assert s.count(anchor)==1;s=s.replace(anchor,(HERE/'recorded_snare_candidate.inc').read_text()+'\n'+anchor,1)
paths['HouseStadiumDetailMood.cpp'].unlink();paths['HouseStadiumDetailMood.cpp'].write_text(s);p=ROOT/'SourceArt/StadiumDetail/Draft/HouseStadiumDetailMood.cpp';p.unlink();p.write_text(s)
s=header.read_text();anchor='void CoachOffice(FHallMesh& Solid,FHallMesh& Detail,FHallMesh& Glow);';assert s.count(anchor)==1;s=s.replace(anchor,anchor+'\n/** CC0 acoustic snare cadence, generated once on the existing worker; normal timers remain unchanged. */\nTArray<int16> SnareCadence();',1);header.unlink();header.write_text(s);p=ROOT/'SourceArt/StadiumDetail/Draft/HouseStadiumDetail.h';p.unlink();p.write_text(s)
p=paths['HouseCampusFall.cpp'];s=p.read_text();a='    const bool bStadiumGhost=HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=7;';assert s.count(a)==1;s=s.replace(a,a+'\n    if(bStadiumGhost)return HouseStadiumDetail::SnareCadence();',1);p.unlink();p.write_text(s)
# The temporal capture exposed nearly invisible program printing. Strengthen only the new mood-paper cover.
p=paths['HouseStadiumDetailLook.cpp'];s=p.read_text();a=s.index('  // Printed, folded homecoming program.');b=s.index('  FHallImage Stripe;',a);part=s[a:b];part=part.replace('.57f,.51f,.36f','.82f,.75f,.58f').replace('.095f,.046f,.13f','.022f,.008f,.035f').replace('.12f,.075f,.075f','.04f,.02f,.035f').replace('.17f,.10f,.12f','.06f,.03f,.055f').replace('53,40,','53,44,').replace('103,32,','103,36,');assert part!=s[a:b];s=s[:a]+part+s[b:];p.unlink();p.write_text(s);p=ROOT/'SourceArt/StadiumDetail/Draft/HouseStadiumDetailLook.cpp';p.unlink();p.write_text(s)
plan=ROOT/'Saved/StadiumDetailWork/final_plan_20261019_a5.json';assert not plan.exists();x=json.loads((ROOT/'Saved/StadiumDetailWork/final_plan_20261019_a4.json').read_text());x['capture_tag']='stadium_s7_20261019_a5';x['performance']=x['performance'].replace('perf4','perf5');
for k in ['sound_native','sound_pcm','sound_inspection']:x[k]=x[k].replace('sound5','sound6')
x['office_walk']=x['office_walk'].replace('walk6','walk7');plan.write_text(json.dumps(x,indent=2)+'\n')
print('Installed recorded snare in owned Mood.cpp, called only in enabled7 legacy branch. Hum/render bytes unchanged. Program-print contrast strengthened. No new Content import, module or lights. All final gates must be fresh.')
