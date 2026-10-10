from pathlib import Path
import json, shutil, hashlib
root=Path('/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-motion-v3')
project=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff')
report=json.loads((root/'native-motion/Saved/HousePigMotionSmoke/result.json').read_text())
assert report['passed'],report
assert not report.get('gripOnly',False),'Full movement and pickup validation is required'
assert all(p['handContactBeforeLift'] for p in report['pigs']),'Pickup must make hand contact before lifting'
for kind,marker in [('motion','HOUSE_PIG_MOTION_SMOKE_RESULT passed=1'),('stairs','HOUSE_PIG_STAIR_SMOKE_RESULT passed=1'),('performance','HOUSE_WALK_PERF_RESULT: passed=1')]:
    assert marker in (root/('native-'+kind+'.log')).read_text(errors='replace'),kind
map_path=project/'Content/Story/Maps/Chapter01_House.umap'
hash_of=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert hash_of(map_path)==hash_of(root/'backup/Chapter01_House.umap'),'Map changed unexpectedly'
archive=project/'SourceArt/PigstyRoom/MotionV3'
(archive/'Source').mkdir(parents=True,exist_ok=True);(archive/'Validation').mkdir(exist_ok=True)
files=['HouseRoamingPig.h','HouseRoamingPig.cpp','HousePigSmoke.cpp','StoryCampaign.h','StoryCampaign.cpp','SeniorDouliAnim.h','SeniorDouliAnim.cpp','SeniorBraxtonVisual.h','SeniorBraxtonVisual.cpp','SeniorPigHoldPose.h']
for name in files:shutil.copy2(project/'Source/SeniorSendoff'/name,archive/'Source'/name)
for p in (root/'source').glob('*'):
    if p.is_file():shutil.copy2(p,archive/'Source'/p.name)
for p in root.glob('build*.log'):shutil.copy2(p,archive/'Validation'/p.name)
for p in root.glob('native-*.log'):shutil.copy2(p,archive/'Validation'/p.name)
shutil.copytree(root/'native-motion/Saved/HousePigMotionSmoke',archive/'Validation/Motion',dirs_exist_ok=True)
report['mapPreservedSha256']=hash_of(map_path);report['sourceSha256']={n:hash_of(project/'Source/SeniorSendoff'/n) for n in files}
(root/'final-review.json').write_text(json.dumps(report,indent=2));shutil.copy2(root/'final-review.json',archive/'Validation/final-review.json')
readme='''# Pig movement and single back leg pickup\n\nMotionV3 preserves the approved pig art, sizes, health, room, mattress, food and door layout.\n\nThe pigs use a four beat walk blending into a diagonal trot. Hooves plant in world space, with two link leg IK, limited floor sampling, turning weight shifts, restrained head motion, and smoother activity transitions.\n\nStand close to a rear leg and press E to start a visible reach and bend, then a gradual lift. Pickup requires anatomically reachable contact; the local hand must touch the ankle before the body moves. The player grips one back leg with the right hand and curled fingers. The other legs hang with gravity; the suspended body has bounded, damped weight response. The actual animated ankle drives first person hand IK and Braxton's visible body. Equipped weapons hide while holding and return on release.\n\nE on clear floor begins an animated lowering before the pig is released. Full body translation and rotation collision and supported placement remain required. E can still open deliberately aimed doors while carrying.\n\nValidation contains observed walk and trot hoof contacts, actual E reach/lift/grip/carry/lower for both sizes, rendered right hand contact, closed door rejection, and saved screenshots. Both pigs also physically descended the stairs and reached the great room. The separate walking performance profile passed. Source contains exact final code and local check tools. Setup changes use isolated test saves and leave the saved map byte for byte unchanged.\n'''
(archive/'README.md').write_text(readme)
main=project/'SourceArt/PigstyRoom/README.md';s=main.read_text();h='\n## Movement and pickup refinement\n';s=s.split(h)[0]+h+'\nMotionV3 adds grounded walk and trot, visible single back leg grasp, gradual lift and lowering, hand contact, and damped suspended motion. Its Source and Validation folders contain the exact code and successful native runtime checks. The approved map and art are preserved.\n';main.write_text(s)
print('PIG_MOTION_ARCHIVE_COMPLETE',archive)
