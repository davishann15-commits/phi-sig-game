"""Assemble final native acceptance only from actual gated evidence and manual inspections.

This cannot mark images or audio inspected. The supplied inspection receipts must
have been written after actual review. Recovery export/full Content audit remains
separate and must succeed before reporting the stadium job finished.
"""
from datetime import datetime,timezone
from pathlib import Path
import argparse,json
from stage_runner import ROOT,WORK,RUNS,PARTS,NAMES,idle,manifest,sha,review_result


def main():
 p=argparse.ArgumentParser();p.add_argument('tag');p.add_argument('--plan',type=Path,required=True);a=p.parse_args()
 idle();frozen=manifest();plan=json.loads(a.plan.read_text());ledger=json.loads((WORK/'acceptance_working.json').read_text())
 assert len(NAMES)==41 and set(ledger['stages'])=={str(n) for n in range(1,7)}
 required={'capture_tag','performance','exact_controls','disable_unity','multiplayer_native','multiplayer_inspection','sound_native','sound_pcm','sound_inspection','night_stadium','night_inspection','office_walk'}
 assert set(plan)==required
 def read(name):
  path=ROOT/plan[name];assert path.is_file(),path
  return path,json.loads(path.read_text())
 capture=plan['capture_tag'];phase_paths={phase:f'Saved/StadiumDetailWork/{capture}_{phase}.json' for phase in ('capture','visual_inspection','reviews')}
 current={k:json.loads((ROOT/v).read_text()) for k,v in phase_paths.items()}
 assert all(d['stage']==7 and d['source_config_map_sha']==frozen for d in current.values())
 assert current['visual_inspection']['all_images_inspected'] and current['visual_inspection']['accepted_for_stage']
 assert current['visual_inspection']['capture_receipt_sha']==sha(ROOT/phase_paths['capture'])
 assert set(current['reviews']['native_results'])==set(PARTS)
 for part in PARTS:assert review_result(RUNS/(capture+'_'+part),part)==current['reviews']['native_results'][part]
 assert all([x['view'] for x in run['frames']]==NAMES for run in current['capture']['runs'])
 data={key:read(key) for key in required-{'capture_tag'}}
 for key in ('performance','exact_controls','disable_unity'):
  assert data[key][1]['passed'] and data[key][1]['source_config_map_sha']==frozen,key
 assert data['performance'][1]['stage']==7 and len(data['performance'][1]['views'])==41
 office=data['office_walk'][1]
 assert office['passed'] and office['stage']==7 and office['source_config_map_sha']==frozen
 assert 'PASS reached=8/8 doors=4/4 indoor=3/3 shellRays=4' in office['summary']
 native=data['multiplayer_native'][1];manual=data['multiplayer_inspection'][1]
 assert native['native_passed'] and native['source_config_map_sha']==frozen and native['normal_lobby'] and not native['forced_scene_clock']
 assert manual['passed'] and manual['all_images_inspected'] and manual['source_config_map_sha']==frozen
 assert manual['native_receipt_sha']==sha(data['multiplayer_native'][0])
 assert manual['inspected_images']==native['images']
 sound=data['sound_native'][1];pcm=data['sound_pcm'][1];manual_sound=data['sound_inspection'][1]
 assert sound['native_passed'] and sound['source_config_map_sha']==frozen and pcm['native_and_pcm_passed']
 assert pcm['tag']==sound['tag'] and len(pcm['images'])==25
 assert manual_sound['passed'] and manual_sound['all_images_inspected'] and manual_sound['actual_spatial_audio_reviewed']
 assert manual_sound['source_config_map_sha']==frozen and manual_sound['native_receipt_sha']==sha(data['sound_native'][0])
 assert manual_sound['pcm_receipt_sha']==sha(data['sound_pcm'][0]) and manual_sound['inspected_images']==pcm['images']
 assert manual_sound['actual_audio_previews']
 for item in manual_sound['actual_audio_previews']:assert sha(ROOT/item['file'])==item['sha256']
 night=data['night_stadium'][1];manual_night=data['night_inspection'][1]
 assert night['passed'] and night['stage']==7 and night['source_config_map_sha']==frozen
 assert night['actual_pinned_nightfall_verified'] and night['native_walking_checks']==65 and night['native_tour_entries']==69
 assert manual_night['passed'] and manual_night['all_images_inspected'] and manual_night['source_config_map_sha']==frozen
 assert manual_night['native_receipt_sha']==sha(data['night_stadium'][0]) and manual_night['inspected_images']==night['images']
 for records in (native['images'],pcm['images'],night['images']):
  for item in records:assert sha(ROOT/item['file'])==item['sha256'],item['file']
 handoff=WORK/(a.tag+'_handoff_reviews.json');output=WORK/(a.tag+'_native_acceptance.json')
 assert not handoff.exists() and not output.exists()
 evidence=[str(path.relative_to(ROOT)) for path,d in data.values()]
 evidence += [item['file'] for item in manual_sound['actual_audio_previews']]
 for paths in ledger.get('stage_additional_inspections',{}).values():evidence.extend(paths)
 for run_tags in ledger.get('stage_additional_runs',{}).values():
  for tag in run_tags:evidence.extend(str(p.relative_to(ROOT)) for p in (RUNS/tag).rglob('*') if p.is_file())
 evidence.extend(str(p.relative_to(ROOT)) for p in WORK.glob('build_stage*.log'))
 tags=[capture+'_'+part for part in PARTS]+[native['tag'],sound['tag'],night['tag'],office['tag']]
 handoff.write_text(json.dumps({'passed':True,'source_config_map_sha':frozen,'finished_utc':datetime.now(timezone.utc).isoformat(),'run_tags':tags,'evidence_files':evidence,'original_review_parts':PARTS,'normal_multiplayer_images_reviewed':True,'native_spatial_pcm_and_actual_listening_reviewed':True,'actual_night_tour_and_images_reviewed':True,'actual_office_walk_door_indoor_shell_passed':True},indent=2)+'\n')
 final={k:v for k,v in ledger.items() if k in ('stages','stage_performance')}
 final['stages']['7']=phase_paths;final['stage_performance']['7']=plan['performance']
 final.update({k:plan[k] for k in ('performance','exact_controls','disable_unity','office_walk')})
 final.update({'passed':True,'source_config_map_sha':frozen,'finished_utc':datetime.now(timezone.utc).isoformat(),'handoff_reviews':str(handoff.relative_to(ROOT)),'additional_evidence_files':evidence,'scope':'All seven native/visual/performance stages and final builds/controls/MP/audio/night gates. Full old Content hash audit and verified recovery export remain required before reporting the task complete.','recovery_pending':True})
 output.write_text(json.dumps(final,indent=2)+'\n');print('Native acceptance assembled; run recovery.py and inspect its actual passed receipt next:',output)


if __name__=='__main__':main()
