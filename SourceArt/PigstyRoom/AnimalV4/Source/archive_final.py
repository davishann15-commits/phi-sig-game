from pathlib import Path
import shutil,json,hashlib
root=Path('/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-animal-v4')
project=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff')
target=project/'SourceArt/PigstyRoom/AnimalV4'
for check,marker in [('animal','HOUSE_PIG_ANIMAL_SMOKE_RESULT passed=1'),('motion','HOUSE_PIG_MOTION_SMOKE_RESULT passed=1'),('stairs','HOUSE_PIG_STAIR_SMOKE_RESULT passed=1')]:
 assert marker in(root/('native-'+check+'.log')).read_text(errors='replace'),check
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
mapfile=project/'Content/Story/Maps/Chapter01_House.umap'
assert sha(mapfile)==sha(root/'backups/Chapter01_House.umap'),'Unrequested map edit'
for sub in ['Source','Art','Audio','Validation']: (target/sub).mkdir(parents=True,exist_ok=True)
source={}
for name in ['HouseRoamingPig.cpp','HouseRoamingPig.h','HousePigSmoke.cpp','StoryCampaign.cpp']:
 p=project/'Source/SeniorSendoff'/name;shutil.copy2(p,target/'Source'/name);source[name]=sha(p)
for p in(root/'source').glob('*.py'):shutil.copy2(p,target/'Source'/p.name)
shutil.copytree(root/'art',target/'Art',dirs_exist_ok=True)
for name in ['PIG_Snort.wav','PIG_Snort_Record.mp3','source-license.json']:
 shutil.copy2(root/'audio'/name,target/'Audio'/name)
for check in ['animal','motion','stairs']:
 shutil.copy2(root/('native-'+check+'.log'),target/'Validation'/('native-'+check+'.log'))
 for p in(root/('native-'+check)/'Saved').glob('HousePig*Smoke'):
  if p.is_dir():shutil.copytree(p,target/'Validation'/p.name,dirs_exist_ok=True)
for name in ['installed.json','build.log']:shutil.copy2(root/name,target/'Validation'/name)
assets={}
for name in ['SM_PIG_BlackHead','SM_PIG_LeftEar','SM_PIG_RightEar','PIG_Snort']:
 p=project/'Content/SeniorSendOff/House/PigstyRoom'/(name+'.uasset');assets[name]=sha(p)
report=dict(sourceSHA256=source,assetSHA256=assets,mapSHA256=sha(mapfile),mapPreserved=True,validation=['autonomous animal behavior and audio playback','physical hoof and carry regression','door-gated stairs traversal'])
(target/'final-review.json').write_text(json.dumps(report,indent=2))
(target/'README.md').write_text("""# Pig animal motion refinement

The approved Pepper and Coal models, sizes, health, room, mattress, food and house map are preserved.

Walking now has gentle side-to-side weight transfer, individual continuous stride and pace variation, softer acceleration and turns, and restrained head counter-motion. Hooves remain planted in world space with their existing anatomical IK. Route starts choose an initially clear floor join around sleeping companions, and speed eases near intermediate turns so tight stair waypoints remain reachable.

Both pigs autonomously stop on safe flat floor for short nose-down sniff bouts, then resume their saved route. Doorways, stairs, feeding, sleep and frightened escape journeys take precedence. Newly requested journeys get individually staggered curiosity timing.

The original approved ear geometry is partitioned from the head with all 19,500 triangles preserved. Separate ear hinges use independently damped bounce and lag during running, with slightly livelier motion on Pepper.

Short spatial snorts use JarredGibb's Pig - Snort 6 field recording, available under CC0: https://freesound.org/people/JarredGibb/sounds/233165/ . The source high-quality preview is converted to mono PCM, with gentle individual pitch and volume variation and room occlusion. Source license and conversion details are in Audio/source-license.json.

Controls remain E near a rear ankle to lift by one back leg and E to lower onto supported clear floor.

Validation contains runtime captures and reports for autonomous sniffing, active audio, ear travel and walking sway, plus the full planted-hoof/carry regression and physical stairs traversal. The saved house map hash is unchanged.
""")
readme=project/'SourceArt/PigstyRoom/README.md'
if readme.exists()and'AnimalV4'not in readme.read_text():
 with readme.open('a')as f:f.write('\n## Animal movement refinement\n\nSee AnimalV4 for natural weight transfer, ground sniffing, spatial snorts, independent ear flop, and runtime validation.\n')
print('PIG_ANIMAL_ARCHIVE_COMPLETE',target)
