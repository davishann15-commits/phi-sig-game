import json, os, unreal
OUT=os.path.join(unreal.Paths.project_saved_dir(),'ClaudeDump','survey4.json')
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
world=unreal.EditorLevelLibrary.get_editor_world()
def v3(v): return [round(v.x,1),round(v.y,1),round(v.z,1)]
out=[]
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    try: o,e=a.get_actor_bounds(False)
    except Exception: continue
    if e.x>2000 or e.y>2000: continue
    if o.x+e.x>=-2995 and o.x-e.x<=-2320 and o.y+e.y>=-800 and o.y-e.y<=-440 and o.z+e.z>=290 and o.z-e.z<=590:
        c=a.get_components_by_class(unreal.StaticMeshComponent)
        out.append({'label':a.get_actor_label(),'cls':a.get_class().get_name(),'folder':str(a.get_folder_path()),'o':v3(o),'e':v3(e),'mesh':c[0].static_mesh.get_name() if c and c[0].static_mesh else None,'col':str(c[0].get_collision_profile_name()) if c else None})
# complex traces against everything (pawn channel) along Y at several X
hits=[]
for x in (-2900,-2700,-2500,-2400):
    for z in (420,):
        for sy,ey in ((-1100,-300),(-300,-1100)):
            h=unreal.SystemLibrary.line_trace_multi(world,unreal.Vector(x,sy,z),unreal.Vector(x,ey,z),unreal.TraceTypeQuery.TRACE_TYPE_QUERY2,True,[],unreal.DrawDebugTrace.NONE,True)
            for hh in (h or []):
                t=hh.to_tuple(); hits.append([x,sy,ey,round(t[4].y,1),t[9].get_actor_label() if t[9] else None])
for y in (-700,-600,-520):
    for sx,ex in ((-2700,-2200),(-2700,-3100)):
        h=unreal.SystemLibrary.line_trace_multi(world,unreal.Vector(sx,y,420),unreal.Vector(ex,y,420),unreal.TraceTypeQuery.TRACE_TYPE_QUERY2,True,[],unreal.DrawDebugTrace.NONE,True)
        for hh in (h or []):
            t=hh.to_tuple(); hits.append(['Y',y,sx,ex,round(t[4].x,1),t[9].get_actor_label() if t[9] else None])
json.dump({'actors':out,'hits':hits},open(OUT,'w'),indent=1)
unreal.log('CLAUDE_SURVEY4_OK')
