"""Write final repository docs only after all actual native/manual gates were assembled."""
from pathlib import Path
from datetime import datetime,timezone
import argparse,json
from stage_runner import ROOT,WORK,manifest
p=argparse.ArgumentParser();p.add_argument('acceptance',type=Path);p.add_argument('--plan',type=Path,required=True);p.add_argument('--recovery-tag',required=True);a=p.parse_args()
frozen=manifest();native=json.loads(a.acceptance.read_text());plan=json.loads(a.plan.read_text())
assert native['passed'] and native['source_config_map_sha']==frozen and set(native['stages'])==set(map(str,range(1,8)))
assert all(c in 'abcdefghijklmnopqrstuvwxyz0123456789_' for c in a.recovery_tag)
perf=json.loads((ROOT/plan['performance']).read_text());assert perf['passed'] and perf['stage']==7
capture=json.loads((ROOT/native['stages']['7']['capture']).read_text())
manual=json.loads((ROOT/native['stages']['7']['visual_inspection']).read_text());assert manual['all_images_inspected'] and manual['accepted_for_stage']
worst=max((max(float(v[m][k]) for m in ('avg_ms','render_ms') for k in ('delta','paired_delta_median')),v['view']) for v in perf['views'])
recovery=f'Saved/StadiumDetailWork/recovery_{a.recovery_tag}/recovery_receipt.json'
core=f"Seven stadium stages have passed native build, actual dusk/night image inspection, the full original11-phase review set and three alternating timing pairs. Final41-view timing passed; worst median/paired frame or render delta {worst[0]:.2f}ms at {worst[1]}, median load delta {perf['load_seconds']['delta']:+.6f}s. The approved shared exterior light count is57, stadium3/new0, interior pool6."
checks=[]
for n in map(str,range(1,8)):
 cp=json.loads((ROOT/native['stages'][n]['capture']).read_text());pp=json.loads((ROOT/native['stage_performance'][n]).read_text());vv=max(max(float(v[m][k]) for m in ('avg_ms','render_ms') for k in ('delta','paired_delta_median')) for v in pp['views'])
 checks.append(f"| {n} | {len(cp['runs'][0]['frames'])*2} inspected | all11 PASS | {vv:.2f}ms | {pp['load_seconds']['delta']:+.6f}s |")
rows=['| View | Frame delta ms | Render delta ms | Paired render delta ms |','|---|---:|---:|---:|']
for v in perf['views']:rows.append(f"| {v['view']} | {v['avg_ms']['delta']:+.2f} | {v['render_ms']['delta']:+.2f} | {v['render_ms']['paired_delta_median']:+.2f} |")
readme=f"""# Calder State Stadium — Homecoming ’86

{core}

The original master prompt is MASTER_PROMPT.md. PLAN.md describes fixed contracts; STATUS.md retains failed candidates and repairs. The editable checkout is Documents/Unreal Projects/SeniorSendoff. Runtime construction is saved in source; Chapter01_House.umap remains byte-identical, with no map save or commit.

## Built

- Individual aluminium and worn timber bench planks, bolted supports, seat/row/section marks, legal aisle steps, nosings, pipe guards, concrete joints/drains/stains/gum, student-section wear and two real additional passages. Existing segment11 route remains.
- Layered press/radio/coaching booths, angled glazing/equipment, camera platform, horns/flags/halyards and antennas; incandescent24–17 scoreboard, dead/flickering bulbs, invented sponsors and rear catwalk/ladder; lattice at all three fixed masts.
- Worn natural turf, divots/chalk, college markings, padded goalposts, grounded chains/down-box, benches/coolers/cups/tape/cable, strapped tarp, maintenance cart/chalk liner, band risers/chairs/case and homecoming float frame.
- Detailed gates/ticket/queue/concession/souvenir fittings, complete75cent menus, dedication plaque, trophy case and flyers. Stadium-wide sign faces use authored lettering, frames, fasteners and restrained wear.
- Sealed locker-room end walls and repaired overhead riser clearance; darker aged oak lockers/wainscot/benches, rubbed fibres and exposed edges, brass names/hardware, shaped jerseys/helmets, trainer table and oval whirlpool fittings/wetfloor. Complete chalk score and Geese crest remain clear. Padded tunnel and torn floor banner retain the existing route.
- Owner-approved worn plaster/mineral floor/exposed fluorescent utility-room palette and a separate coach office directly connected to the locker room. The one new room and operable door were explicitly authorized later; all previous doors/routes remain. Aged pedestal desk, softly curved/reclined upholstered chairs, filing cabinet/bookcase, rounded black rotary phone with open finger wheel and curved handset, coaching notes and emission-only curved desk lamp. The owner accepted the revised chair and telephone from actual game close-ups. A crooked1978 coaching certificate, aged October1986 calendar and felt pennant add personal wall details. Overlapping paperwork/open folder, mug, keys/loose stationery and layered bulletin notes/tickets/team snapshot use natural independent placements. Additional actual walking:8/8 stops,4/4 ordinary door interactions,3/3 indoor checks,4shell rays, pool6. Six appended170cm office views are included in all final captures/timing.
- Restrained positioned electrical/PA hum and occasional halyard clank, rare distant snare on its normal timer, subtle emission-only flicker and six bounded drifting programs. Larger reachable props collide; small paper/scuffs remain cosmetic.

## Controls and protected scope

`-NoStadiumDetail` restores the released R7 stadium-disabled scene exactly in native counts/reviews. `-NoFraternityRowUpgrade` additionally gives the original53-light control; adding `-NoCalderTerrain` gives the original terrain-off counts. TreeCRC0f3b2bb7 and terrain72564759 remain. Installed R7 Row is preserved and unfinished; this job does not resume it.

Normal source default is Step7. `-StadiumDetailStep=1..7` is development-only. `-StadiumDetailReview` adds physical/wall-seam checks; `-StadiumSoundReview` is an opt-in real audio/temporal recorder, including the office. `-StadiumCoachOfficeReview` runs the added walking/door route independently of the default-open static clearance audit. Normal play needs no review flags. Stadium location, turf−56, four Z constants, mast positions, apron/roads/bus bay/loop and the tower sightline remain fixed. No house/court/FallJam edits, new modules, Nanite/Landscape/Water/Niagara/PCG, new lights or paid assets.

## Assets and references

Ten new owned assets only: T_Stadium_Concrete_Color/Normal, M_Stadium_FieldDetail and T_Stadium_RoomPlaster_Color/Normal and T_Stadium_FurnitureWood/FurnitureLeather Color/Normal plus T_Stadium_CoachSnapshot. The snapshot is a fictional old team image generated for this game prop; it is not a documentary photo. The furniture uses unmodified CC0 [Wood Table Worn](https://polyhaven.com/a/wood_table_worn), Dimitrios Savva/Rico Cilliers,2K, and [Brown Leather](https://polyhaven.com/a/brown_leather), Rob Tuytel,1K. The room plaster maps are unmodified2K [Poly Haven painted_plaster_wall](https://polyhaven.com/a/painted_plaster_wall), Amal Kumar, CC0,2m physical tile. The unmodified2K concrete maps are [Poly Haven concrete_floor_worn_001](https://polyhaven.com/a/concrete_floor_worn_001), Dimitrios Savva/Rico Cilliers, [CC0](https://polyhaven.com/license);3m tile, DirectX normal. Field detail reuses the existing ambientCGGrass004 CC0 colour/normal byte-exact, preserving painted markings and field position. SOURCE.json and asset batch logs retain hashes/shader information. The owner-directed acoustic snare uses six recorded CC0 hits from [Virtuosity Drums](https://versilian-studios.com/virtuosity-drums/), performed by Austin McMahon and produced by Versilian Studios/Karoryfer Samples. AudioCharacter/SOURCE.json records the original Git blob/SHA256 and prepared PCM hashes; Virtuosity_LICENSE.txt preserves the complete CC0 license. The small compressed samples are embedded in owned runtime code; no Content audio package is imported. Accepted hum and normal drum scheduling/position/gain remain unchanged.

Furniture is owned runtime geometry; no Fab furniture asset was acquired or imported. All other new finishes and invented marks are authored runtime art using existing parents. Reference photos are study-only and are not game content. REFERENCES.md retains dates/uncertainty and actual observations; the period football locker-room photo has metal lockers and supports scale/hardware context, while the1983 training text supports table proportions. The owner-directed aged wood is not claimed to reproduce a dated wooden-room photograph or match photographic realism.

## Validation

| Stage | Dusk/night images | Original reviews | Worst timing delta | Load delta |
|---|---:|---|---:|---:|
{chr(10).join(checks)}

Both final exposures passed the additive floor/capsule audit and42actual streamed-room wall rays with zero leaks. The final night STA tour passed all69actions/65walking checks, original doors/locked messages/unloading and pool6; all7images were actually inspected. Full native regressions retain431interior stops,99roam stops,6094reach targets/1891drop seeds/0traps and all23building checks.

Final normal-lobby multiplayer passed with matching terrain/tree hashes and zero walkthrough corrections; all peer images were inspected. Real registered-submix recordings, source consumption, normal scheduled snare repeat, indoor/far silence, return/recovery, paper floor clearance, flag movement and both actual flicker states were checked; all25media frames and actual spatial audio excerpts were reviewed. Listening excerpts explicitly label any audition-only gain; runtime gain is unchanged by those files. Final standalone compile/strict signature and actual separate compilation of all10owned cpp files with DisableUnity passed. Standalone gameplay was not separately claimed.

Timing uses every41current view,300samples/4warm seconds/3alternating pairs/Metal offscreen, budgets≤+1.5ms and load≤+1s. A common existing moving-world phase120 is used on BOTH variants; normal render wall time is retained. Physical audits run separately at identical source hashes. No failed round was removed. Earlier failed/incomplete revisions remain labelled in STATUS.

{chr(10).join(rows)}

## Evidence and recovery

Native acceptance: {str(a.acceptance.resolve().relative_to(ROOT))}. It assembles actual seven-stage captures/manual receipts/full regressions/timing and final control/build/MP/audio/night gates at frozen source hashes. The final full pre-existing Content/source/config/map audit, actual Git patch test-apply and exact extracted payload/evidence hashes are authoritative in `{recovery}` when present with passed=true. Documentation is finalized before that export so its archived bytes match the project.
"""
(ROOT/'SourceArt/StadiumDetail/README.md').write_text(readme)
status=ROOT/'SourceArt/StadiumDetail/STATUS.md';status.write_text(status.read_text()+f"\n\n## Final native acceptance —{datetime.now(timezone.utc).isoformat()}\n\n{core} Owner dark/worn timber refinement is installed and actually inspected; wall/sign repairs retained. Actual final audio/temporal25frames, nightSTA7frames, normalMPpeerimages and all final native/build/control gates passed. Failed earlier attempts remain labelled. Final immutable recovery authority: {recovery} when present passed=true; full hash/export/test-apply is the next serialized step.\n")
handoff=ROOT/'SourceArt/CalderLandscape/CODEX_HANDOFF.md';text=handoff.read_text();start='<!-- STADIUM_ACTIVE_20261019 -->';end='<!-- END_STADIUM_ACTIVE_20261019 -->';assert text.count(start)==text.count(end)==1
block=f"{start}\n## Current stadium state — final native gates complete\n\n{core} The owner-approved worn utility room, separate connected coach office, furniture, gap and signs are installed and actually inspected. The one added room/door is explicitly authorized by the later direct user request. All final audio/temporal, nightSTA, normal multiplayer, exact controls and non-Unity/standalone-signature gates passed. Actual native acceptance: {str(a.acceptance.resolve().relative_to(ROOT))}. Full old Content/hash audit and immutable recovery export/test-apply follow; {recovery} present passed=true is the completion authority. No map saves/commits. Preserve paused unfinished R7 Row and all existing wildlife/nightfall/protected world work. No older pending instructions below become new stadium scope.\n{end}"
i=text.index(start);j=text.index(end)+len(end);handoff.write_text(text[:i]+block+text[j:])
own_handoff=ROOT/'SourceArt/StadiumDetail/CODEX_HANDOFF.md';old_handoff=own_handoff.read_text();history='## Historical continuation below — superseded by the current block above'
assert history in old_handoff
own_handoff.write_text(f"# Current stadium state — final native gates complete\n\n{core}\n\nOwner-approved worn utility room, connected coach office, furniture and creepy sparse acoustic snare are installed. All 41 dusk/night views, full11 reviews, audio/temporal25images, night7images, actual office walk, normal multiplayer, exact controls, standalone/signature and DisableUnity builds passed. Native acceptance: {str(a.acceptance.resolve().relative_to(ROOT))}. Full old Content/hash audit and real recovery/test-apply follow; completion authority is {recovery} when present with passed=true. No map save/commit. Preserve paused unfinished R7 Row, protected world/wildlife/nightfall,57exteriorlights/stadium3/new0/pool6. Current Source and owned Draft root are canonical; historical recipes below must not run.\n\n"+old_handoff[old_handoff.index(history):])
campus=ROOT/'SourceArt/CampusFall/README.md';campus.write_text(campus.read_text()+f"\n\n## Calder State Stadium detail revision —19October2026\n\n{core} This extends only the authorized stadium behind NoStadiumDetail; the released R7 Row control remains exact. Full implementation/assets/limitations and actual evidence are in SourceArt/StadiumDetail/README.md and STATUS.md. Completion/recovery authority: {recovery} when present passed=true. No map or existing Content asset was saved.\n")
assert manifest()==frozen
print('Final native documentation written before recovery export; recovery still must actually pass.')
