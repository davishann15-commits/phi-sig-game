"""Only after actual stage4acceptance and guarded default4->5; no engine may be running."""
from pathlib import Path
import json,shutil,sys
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'SourceArt/StadiumDetail'))
from stage_runner import idle,manifest
idle()
work=ROOT/'Saved/StadiumDetailWork';src=ROOT/'Source/SeniorSendoff';art=ROOT/'SourceArt/StadiumDetail';draft=art/'Draft';prepared=Path(__file__).resolve().parent
advance=json.loads((work/'stage_4_accepted_advance_5.json').read_text())
assert advance['new_default_stage']==5 and advance['new_source_config_map_sha']==manifest()
assert 'int32 Step(){int32 Value=5;' in (src/'HouseStadiumDetail.cpp').read_text()
for p in draft.glob('HouseStadiumDetail*'):
 if p.suffix in ('.h','.cpp'):assert p.read_bytes()==(src/p.name).read_bytes()
views=json.loads((prepared/'concourse_close_views.json').read_text())
assert len(views['views'])==3
camera=src/'HouseExterior.cpp';before=camera.read_text();assert all(v['name'] not in before for v in views['views'])
last=next(l for l in before.splitlines() if 'TEXT("detail_stadium_float_frame")' in l)
def vector(values):return 'FVector('+','.join(f'{x:.3f}' for x in values)+')'
lines=['    // '+views['added_comment']]
for v in views['views']:
 assert v['eye'][2]-v['floor_z']==170
 lines.append('    { TEXT("'+v['name']+'"), '+vector(v['eye'])+', '+vector(v['target'])+', '+str(v['fov'])+'.f },')
assert before.count(last)==1
camera.unlink();camera.write_text(before.replace(last,last+'\n'+'\n'.join(lines)))
assert [l for l in camera.read_text().splitlines() if l not in lines]==before.splitlines()
(draft/'concourse_views.inl').write_text('\n'.join(lines)+'\n')
shutil.copy2(prepared/'concourse_close_views.json',art/'concourse_close_views.json')
(src/'HouseStadiumDetailConcourse.cpp').unlink();shutil.copy2(prepared/'HouseStadiumDetailConcourse.cpp',src/'HouseStadiumDetailConcourse.cpp')
(draft/'HouseStadiumDetailConcourse.cpp').unlink();shutil.copy2(src/'HouseStadiumDetailConcourse.cpp',draft/'HouseStadiumDetailConcourse.cpp')
look=src/'HouseStadiumDetailLook.cpp';look_text=look.read_text();marker=' FHallImage Goose;';assert look_text.count(marker)==1;look.unlink();look.write_text(look_text.replace(marker,(prepared/'flyer_look_block.txt').read_text()+marker));(draft/look.name).unlink();shutil.copy2(look,draft/look.name)
# A code-fit audit found the narrow-end case latch used the wider bell-end depth.
# The full stage5capture/review will recheck the retained field as well as concourse changes.
field=src/'HouseStadiumDetailField.cpp';field_text=field.read_text()
old='for(float X:{-42.f,44.f}){SoftBox(TEXT("SD_Aluminium"),CaseAt({X,19,1}),{9,2,8},CaseYaw,false);Pipe(TEXT("SD_Steel"),CaseAt({X-4,-15,0}),CaseAt({X+4,-15,0}),1.6f,false,9000);}'
new='for(float X:{-42.f,44.f}){const float FaceY=X>20.f?19.f:14.f;SoftBox(TEXT("SD_Aluminium"),CaseAt({X,FaceY+.5f,1}),{9,2,8},CaseYaw,false);Pipe(TEXT("SD_Steel"),CaseAt({X-4,-FaceY-.3f,0}),CaseAt({X+4,-FaceY-.3f,0}),1.6f,false,9000);}'
assert old in field_text;field_text=field_text.replace(old,new).replace('CaseAt({-12,16,0})','CaseAt({-12,14.6f,0})').replace('CaseAt({12,16,0})','CaseAt({12,14.6f,0})');field.unlink();field.write_text(field_text);(draft/field.name).unlink();shutil.copy2(field,draft/field.name)
# Verify that each new170cm-eye view has the prescribed real floor and a clear ordinary capsule.
review=src/'HouseStadiumDetailReview.cpp';review_text=review.read_text();marker='  if(HouseStadiumDetail::Step()>=6)';assert review_text.count(marker)==1
poses=','.join('{'+','.join(f'{x:.3f}' for x in [*v['eye'][:2],v['floor_z']])+'}' for v in views['views'])
audit='  if(HouseStadiumDetail::Step()>=5){const FVector Views[]={'+poses+'};for(const FVector& P:Views){CheckFloor(P,TEXT("concourse-close-view-floor"));FHitResult H;++Sweeps;if(W->SweepSingleByChannel(H,P+FVector(0,0,97),P+FVector(0,0,98),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(36,96),Q)){++Blocked;OK=false;if(Problems.Num()<32)Problems.Add(FString::Printf(TEXT("concourse view capsule blocked at=%s component=%s"),*P.ToString(),*GetNameSafe(H.GetComponent())));}}}\n'
review.unlink();review.write_text(review_text.replace(marker,audit+marker));(draft/review.name).unlink();shutil.copy2(review,draft/review.name)
p=art/'stage_runner.py';t=p.read_text();needle="    view['name'] for view in json.loads((ROOT / 'SourceArt/StadiumDetail/field_close_views.json').read_text())['views']]";assert needle in t;t=t.replace(needle,needle+" + [\n    view['name'] for view in json.loads((ROOT / 'SourceArt/StadiumDetail/concourse_close_views.json').read_text())['views']]");p.unlink();p.write_text(t)
p=art/'recovery.py';t=p.read_text().replace('(21 if int(stage) < 4 else 28)','(21 if int(stage) < 4 else 28 if int(stage) == 4 else 31)');needle="    added_names = ('detail_stadium_portal_east', 'detail_stadium_portal_west', *[view['name'] for view in field_views['views']])";assert needle in t;t=t.replace(needle,"    concourse_views = json.loads((ROOT / 'SourceArt/StadiumDetail/concourse_close_views.json').read_text())\n"+needle[:-1]+", *[view['name'] for view in concourse_views['views']])");t=t.replace('== 9\n','== 12\n').replace("and field_views['added_comment'] not in line]","and field_views['added_comment'] not in line\n                and concourse_views['added_comment'] not in line]");p.unlink();p.write_text(t)
(work/'stage5_concourse_integration.json').write_text(json.dumps({'stage':5,'before_source_config_map_sha':advance['new_source_config_map_sha'],'after_source_config_map_sha':manifest(),'added_views':views,'all_prior_camera_lines_preserved':True,'legacy_routes_changed':False,'build_pending':True},indent=2)+'\n')
print('Stage5concourse refinements installed; every prior28pose preserved; clean build then31viewnativecapture required')
