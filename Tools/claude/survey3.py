import json, os, unreal
OUT=os.path.join(unreal.Paths.project_saved_dir(),'ClaudeDump','survey3.json')
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
world=unreal.EditorLevelLibrary.get_editor_world()
A=unreal.EditorAssetLibrary
def v3(v): return [round(v.x,1),round(v.y,1),round(v.z,1)]
res={'meshes':{},'actors':[],'grid':[]}
for p in ['/Game/SeniorSendOff/House/PigstyRoom/PIG_COL_OriginalShellRemainder','/Game/SeniorSendOff/House/PigstyRoom/PIG_SM_OriginalFloorsRemainder','/Game/SeniorSendOff/House/HallwayEnd/SM_HallEnd_ThreeRoomsShell','/Game/SeniorSendOff/House/HallwayEnd/SM_HallEnd_ThreeRoomsFloors','/Game/SeniorSendOff/House/Room204/R204_SM_InteriorPaint','/Game/SeniorSendOff/House/AcrossPosterBedroom/SM_Bedroom_WoodFloor']:
    m=A.load_asset(p) if A.does_asset_exist(p) else None
    if m: b=m.get_bounds(); res['meshes'][p.split('/')[-1]]={'o':v3(b.origin),'e':v3(b.box_extent)}
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    l=a.get_actor_label()
    if any(k in l for k in ('OriginalShell','OriginalFloors','PIG_Expanded','HallEnd_SM','HallEnd_COL','R204_SM_InteriorPaint','R204_SM_WoodFloor','SM_Upper_','COL_Upper_','Bedroom_WoodFloor','PhotoBedroomCleared')):
        o,e=a.get_actor_bounds(False)
        c=a.get_components_by_class(unreal.StaticMeshComponent)
        res['actors'].append({'label':l,'o':v3(o),'e':v3(e),'vis':bool(c and c[0].is_visible()),'col':str(c[0].get_collision_profile_name()) if c else None,'mesh':c[0].static_mesh.get_name() if c and c[0].static_mesh else None})
def tr(s,e):
    h=unreal.SystemLibrary.line_trace_single(world,s,e,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[],unreal.DrawDebugTrace.NONE,True)
    if h is None: return None
    t=h.to_tuple()
    if not t[0]: return None
    return [round(float(t[3]),1), t[9].get_actor_label() if t[9] else None]
for x in range(-3300,-1300,100):
    for y in range(-2100,700,100):
        p=unreal.Vector(x,y,420)
        d={}
        for k,dv in (('px',(1,0,0)),('nx',(-1,0,0)),('py',(0,1,0)),('ny',(0,-1,0)),('dn',(0,0,-1))):
            r=tr(p,p+unreal.Vector(*dv)*1500)
            d[k]=r
        res['grid'].append({'p':[x,y],'t':d})
json.dump(res,open(OUT,'w'))
unreal.log('CLAUDE_SURVEY3_OK')
