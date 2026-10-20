"""Read-only survey 2: interactive prop configs, mesh asset details, collision actors near the closet and pigsty,
and the photo-matched bedroom/room204 actors as organisation examples."""
import json, os, unreal
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'ClaudeDump', 'survey2.json')
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors = unreal.EditorLevelLibrary.get_all_level_actors()
A = unreal.EditorAssetLibrary
def v3(v): return [round(v.x,3), round(v.y,3), round(v.z,3)]
def rot(r): return [round(r.pitch,3), round(r.yaw,3), round(r.roll,3)]
def val(x):
    if isinstance(x, unreal.Vector): return v3(x)
    if isinstance(x, unreal.Rotator): return rot(x)
    if isinstance(x, unreal.Name): return str(x)
    if isinstance(x, (int,float,bool,str)) or x is None: return x
    if isinstance(x, unreal.Object): return x.get_path_name()
    if isinstance(x, unreal.Array): return [val(i) for i in x]
    try: return str(x)
    except Exception: return '?'
def props(a, names):
    d={}
    for n in names:
        try: d[n]=val(a.get_editor_property(n))
        except Exception as e: pass
    return d
PROP_NAMES=['prop_id','prop_name','intact_mesh','broken_mesh','damaged_mesh','heavily_damaged_mesh','material_overrides','movable','breakable','wall_mounted','broken_version_movable','break_health','detach_health','mass_kg','impact_break_speed','carry_size','damage_category','cloth_flag','rolling_chair']
DOOR_NAMES=['original_leaf','original_glass','broken_frame_mesh','doorway_center','door_id','closed_yaw','open_yaw','locked','door_group','panel_center_world','panel_half_extent','added_handle_mesh','added_handle_world_position','added_handle_rotation','local_leaf_space']
def base(a):
    d={'label':a.get_actor_label(),'class':a.get_class().get_name(),'folder':str(a.get_folder_path()),'location':v3(a.get_actor_location()),'rotation':rot(a.get_actor_rotation()),'scale':v3(a.get_actor_scale3d())}
    try:
        o,e=a.get_actor_bounds(False); d['bo']=v3(o); d['be']=v3(e)
    except Exception: pass
    try: d['tags']=[str(t) for t in a.tags]
    except Exception: pass
    comps=a.get_components_by_class(unreal.StaticMeshComponent)
    if comps:
        c=comps[0]
        d['mesh']=c.static_mesh.get_path_name() if c.static_mesh else None
        d['materials']=[m.get_path_name() if m else None for m in c.get_materials()]
        d['collision']=str(c.get_collision_profile_name()); d['mobility']=str(c.mobility)
        try: d['collision_enabled']=str(c.get_collision_enabled())
        except Exception: pass
    return d
def inbox(d, mn, mx):
    if 'bo' not in d: return False
    o,e=d['bo'],d['be']
    return all(o[i]+e[i]>=mn[i] and o[i]-e[i]<=mx[i] for i in range(3))
res={'interactive_examples':[],'pig_props':[],'closet_region':[],'pigsty_region':[],'doors':[],'bedroom_example':[],'room204':[],'collision_folder':[],'lights_near':[],'meshes':{}}
CLOSET_MIN=(-3000,700,-400); CLOSET_MAX=(-2450,1200,-60)
PIG_MIN=(-3050,-1450,250); PIG_MAX=(-2250,-700,620)
examples={'Interactive_BR_SM_Bedroom_StainlessMiniFridge__001','Interactive_R204_QuiltedLeatherFuton__001','Interactive_R204_LargeMottledGrayRug__001','Interactive_BR_SM_Bedroom_WallTelevision__001','Interactive_Library_LongRug_001','Interactive_Library_PaperCup_001','Interactive_Library_DarkThrowPillow_001','Interactive_Library_CurvedWoodDeskChair_001','Interactive_Library_DeskMonitor_001','Interactive_R204_FlushCeilingLight__001','Interactive_Library_FramedHistoricPhoto_001','Interactive_BR_BedDetail_SM_BedProps_Dresser_26_001__001'}
for a in actors:
    d=base(a); lab=d['label']; cls=d['class']; folder=d['folder']
    if cls=='HouseInteractiveProp':
        if lab.startswith('Interactive_PIG_'): d.update(props(a,PROP_NAMES)); res['pig_props'].append(d)
        elif lab in examples or 'Futon' in lab or 'MiniFridge' in lab or 'Rug' in lab and 'R204' in lab or 'CeilingLight' in lab or 'Dresser' in lab or 'TrashCan' in lab or 'Blinds' in lab or 'PaperTowel' in lab:
            d.update(props(a,PROP_NAMES)); res['interactive_examples'].append(d)
    if isinstance(a, unreal.ExistingHouseDoor) and (lab in ('Library_MechanicalServiceDoor','HallEnd_West') ):
        d.update(props(a,DOOR_NAMES)); res['doors'].append(d)
    if inbox(d,CLOSET_MIN,CLOSET_MAX) and not folder.endswith('LooseItems'): res['closet_region'].append(d)
    if inbox(d,PIG_MIN,PIG_MAX): res['pigsty_region'].append(d)
    if folder.startswith('Second floor/Photo matched bedroom'): res['bedroom_example'].append(d)
    if folder.startswith('Second floor/Photo matched Room 204'): res['room204'].append(d)
    if folder=='House/Collision': res['collision_folder'].append(d)
    if cls in ('PointLight','SpotLight','RectLight') and (inbox(d,CLOSET_MIN,CLOSET_MAX) or inbox(d,PIG_MIN,PIG_MAX) or (d['location'][0]<-2300 and d['location'][1]>600 and d['location'][2]<0)):
        lc=a.get_components_by_class(unreal.LightComponent)
        if lc:
            l=lc[0]; d['intensity']=l.intensity; d['color']=str(l.light_color); d['units']=str(getattr(l,'intensity_units','?')) ; d['attenuation']=getattr(l,'attenuation_radius',None); d['cast_shadows']=l.cast_shadows
        res['lights_near'].append(d)
for path in ['/Game/SeniorSendOff/House/BasementLibrary/SM_Lib_ServiceAnnexShell','/Game/SeniorSendOff/House/PigstyRoom/PIG_SM_ExpandedRoom','/Game/SeniorSendOff/House/PigstyRoom/PIG_COL_ExpandedRoom','/Game/SeniorSendOff/House/PigstyRoom/PIG_SM_Mattress','/Game/SeniorSendOff/House/PigstyRoom/PIG_SM_FeedBucket','/Game/SeniorSendOff/House/PigstyRoom/PIG_SM_KeepShutPlaque','/Game/SeniorSendOff/House/BasementLibrary/SM_Lib_MechanicalDoorSource','/Game/SeniorSendOff/House/Room204/SM_R204_QuiltedLeatherFuton','/Game/SeniorSendOff/House/Room204/SM_R204_LargeMottledGrayRug','/Game/SeniorSendOff/House/AcrossPosterBedroom/SM_Bedroom_StainlessMiniFridge','/Game/SeniorSendOff/House/AcrossPosterBedroom/SM_BedProps_Dresser','/Game/SeniorSendOff/House/AcrossPosterBedroom/SM_BedProps_CoffeeTable','/Game/SeniorSendOff/House/AcrossPosterBedroom/SM_Bedroom_OvalCoffeeTable','/Game/SeniorSendOff/House/ReferenceKitchen/SM_Kitchen_WindowBlinds','/Game/SeniorSendOff/House/ReferenceKitchen/SM_Kitchen_TrashCan','/Game/SeniorSendOff/House/ReferenceKitchen/SM_Kitchen_PaperTowelRoll','/Game/SeniorSendOff/House/ReferenceKitchen/SM_Kitchen_StainlessApplianceRack','/Game/SeniorSendOff/House/ReferenceKitchen/SM_Kitchen_ClearCup','/Game/SeniorSendOff/House/BasementLibrary/SM_Lib_CurvedWoodDeskChair','/Game/SeniorSendOff/House/BasementLibrary/SM_Lib_MeshOfficeChair','/Game/SeniorSendOff/House/BasementLibrary/SM_Lib_DeskMonitor','/Game/SeniorSendOff/House/BasementLibrary/SM_Lib_Loose_DeskRiser_notebook','/Game/SeniorSendOff/House/BasementLibrary/SM_Lib_DarkThrowPillow','/Game/SeniorSendOff/House/BasementLibrary/SM_Lib_PaperCup','/Game/SeniorSendOff/House/BasementLibrary/SM_Lib_WhitePlasticDrawerTower','/Game/SeniorSendOff/House/BasementLibrary/SM_Lib_LongRug','/Game/SeniorSendOff/House/Room204/SM_R204_FlushCeilingLight','/Game/SeniorSendOff/House/Room204/SM_R204_GalanzFridgeAndMicrowave','/Game/SeniorSendOff/House/Room204/SM_R204_OpenDarkWoodDresser','/Game/SeniorSendOff/House/Room204/SM_R204_FutonClutter','/Game/SeniorSendOff/House/Room204/SM_R204_NightstandAndLamp','/Game/SeniorSendOff/House/AcrossPosterBedroom/SM_BR_PurplePerimeterLED','/Game/SeniorSendOff/House/AcrossPosterBedroom/SM_Bedroom_WoodFloor','/Game/SeniorSendOff/House/Room204/SM_R204_WoodFloor','/Game/SeniorSendOff/House/Room204/R204_SM_InteriorPaint','/Game/SeniorSendOff/House/PigstyRoom/PIG_SM_WaterTrough','/Game/SeniorSendOff/House/PigstyRoom/PIG_SM_OutsideDoorPlaque','/Game/SeniorSendOff/House/PigstyRoom/PIG_SM_Blanket','/Game/SeniorSendOff/House/PigstyRoom/PIG_SM_CheetosBag','/Game/SeniorSendOff/House/PigstyRoom/PIG_SM_OrangeCrumbs','/Game/SeniorSendOff/House/PigstyRoom/PIG_SM_BrokenBoards','/Game/SeniorSendOff/House/PigstyRoom/PIG_SM_BulkheadLamp','/Game/SeniorSendOff/House/PigstyRoom/PIG_SM_StrawPatch','/Game/SeniorSendOff/House/PigstyRoom/PIG_SM_WallVent','/Game/SeniorSendOff/House/PigstyRoom/PIG_SM_GrimeScratch','/Engine/BasicShapes/Cube','/Engine/BasicShapes/Cylinder','/Engine/BasicShapes/Sphere','/Engine/BasicShapes/Plane']:
    m=A.load_asset(path) if A.does_asset_exist(path) else None
    if not m: res['meshes'][path]='MISSING'; continue
    try:
        b=m.get_bounds(); bs=m.get_editor_property('body_setup')
        info={'origin':v3(b.origin),'extent':v3(b.box_extent),'sections':m.get_num_sections(0),'materials':[ (str(s.material_slot_name), s.material_interface.get_path_name() if s.material_interface else None) for s in m.static_materials]}
        if bs:
            info['trace_flag']=str(bs.get_editor_property('collision_trace_flag'))
            ag=bs.get_editor_property('agg_geom')
            info['convex']=len(ag.get_editor_property('convex_elems')); info['boxes']=len(ag.get_editor_property('box_elems')); info['spheres']=len(ag.get_editor_property('sphere_elems'))
        try: info['nanite']=m.get_editor_property('nanite_settings').enabled
        except Exception: pass
        res['meshes'][path]=info
    except Exception as e: res['meshes'][path]='ERR '+str(e)
os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(res, open(OUT,'w'), indent=1)
unreal.log('CLAUDE_SURVEY2_OK closet=%d pigsty=%d pigprops=%d examples=%d'%(len(res['closet_region']),len(res['pigsty_region']),len(res['pig_props']),len(res['interactive_examples'])))
