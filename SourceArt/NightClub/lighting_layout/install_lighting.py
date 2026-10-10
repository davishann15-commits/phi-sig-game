import json, pathlib, re, traceback, unreal

B = pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub'
exec((B/'install.py').read_text().split('def run():')[0], globals())
L = B/'lighting_layout'
D = '/Game/SeniorSendOff/House/NightClub/Lighting'

def vector_parameter(m, name, value):
    n=E.create_material_expression(m, unreal.MaterialExpressionVectorParameter)
    n.set_editor_property('parameter_name',name)
    n.set_editor_property('default_value',unreal.LinearColor(*value,1))
    return n

def scalar_parameter(m, name, value):
    n=E.create_material_expression(m, unreal.MaterialExpressionScalarParameter)
    n.set_editor_property('parameter_name',name)
    n.set_editor_property('default_value',value)
    return n

def link(a,b,name='',output=''):
    names=list(E.get_material_expression_input_names(b))
    if name=='Input' and name not in names:name=names[0]
    if name=='InOpacity' and name not in names:name=next(n for n in names if 'opacity' in n.lower())
    assert E.connect_material_expressions(a,output,b,name),(str(a),str(b),name,names)

def multiply(m,a,b):
    n=E.create_material_expression(m,unreal.MaterialExpressionMultiply)
    link(a,n,'A');link(b,n,'B');return n

def inverse(m,a):
    n=E.create_material_expression(m,unreal.MaterialExpressionOneMinus)
    link(a,n,'Input');return n

def authored_material(name,beam=False):
    m=A.load_asset(D+'/'+name) if A.does_asset_exist(D+'/'+name) else T.create_asset(name,D,unreal.Material,unreal.MaterialFactoryNew())
    E.delete_all_material_expressions(m)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    if beam:
        m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE)
        m.set_editor_property('two_sided',True)
        m.set_editor_property('translucency_pass',unreal.MaterialTranslucencyPass.MTP_BEFORE_DOF)
    color=vector_parameter(m,'BeamColor' if beam else 'LensColor',(.025,.22,1))
    strength=scalar_parameter(m,'BeamStrength' if beam else 'LensStrength',1.0)
    emission=multiply(m,multiply(m,color,strength),const(m,2.0 if beam else 3.0))
    eye=E.create_material_expression(m,unreal.MaterialExpressionEyeAdaptationInverse)
    input_name=next(x for x in E.get_material_expression_input_names(eye) if 'light' in x.lower())
    link(emission,eye,input_name)
    assert E.connect_material_property(eye,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if beam:
        uv=E.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate)
        u=E.create_material_expression(m,unreal.MaterialExpressionComponentMask)
        for channel,value in [('r',True),('g',False),('b',False),('a',False)]:u.set_editor_property(channel,value)
        link(uv,u,'Input')
        fresnel=E.create_material_expression(m,unreal.MaterialExpressionFresnel)
        fresnel.set_editor_property('exponent',1.0);fresnel.set_editor_property('base_reflect_fraction',.0)
        side=inverse(m,fresnel)
        soft_side=multiply(m,side,multiply(m,side,side))
        opacity=multiply(m,multiply(m,inverse(m,u),soft_side),scalar_parameter(m,'BeamOpacity',.05))
        fade=E.create_material_expression(m,unreal.MaterialExpressionDepthFade)
        fade.set_editor_property('fade_distance_default',25.0)
        link(opacity,fade,'InOpacity')
        assert E.connect_material_property(fade,'',unreal.MaterialProperty.MP_OPACITY)
    E.recompile_material(m)
    assert A.save_loaded_asset(m,False),name
    return m

def snapshot(a):
    r=a.get_actor_rotation()
    row=dict(name=a.get_name(),label=a.get_actor_label(),cls=a.get_class().get_name(),location=xyz(a.get_actor_location()),rotation=[r.pitch,r.yaw,r.roll],scale=xyz(a.get_actor_scale3d()),meshes=[],lights=[])
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        if c.static_mesh:
            row['meshes'].append(dict(component=c.get_name(),mesh=c.static_mesh.get_path_name(),materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())],collision=str(c.get_collision_enabled()),visible=c.is_visible()))
    for c in a.get_components_by_class(unreal.LightComponent):row['lights'].append(dict(component=c.get_name(),intensity=c.intensity))
    return row

def run_lighting():
    unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
    unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
    old={a.get_name():snapshot(a) for a in S.get_all_level_actors()}
    actors={a.get_actor_label():a for a in S.get_all_level_actors()}
    club=actors['NightClub_Ambience']
    protected_club_keys=['lower_room_min','lower_room_max','upper_room_min','upper_room_max','lower_door_center','upper_door_center','lower_door_inward_normal','upper_door_inward_normal','pole_hole_center','pole_hole_radius','door_fade_width','door_half_width','music_loop','music_bpm','music_loop_seconds','music_volume','hole_music_gain']
    protected_club={k:club.get_editor_property(k) for k in protected_club_keys}
    assert protected_club['lower_room_max'].x==-1490 and protected_club['upper_room_max'].x==-1490,'Enlarged room bounds missing'
    art=json.loads((B/'lighting_refit/fixture_manifest.json').read_text())
    lens=authored_material('M_NCLF_Lens')
    beam=authored_material('M_NCLF_Beam',True)
    mats={}
    for name,row in art['materials'].items():
        mats[name]=A.load_asset(row['reuse_asset']) if row.get('reuse_asset') else beam if 'Beam' in name else lens
        assert mats[name],name
    meshes={}
    for row in art['assets']:
        mesh=imp(row['fbx'],row['name'])
        slots=list(mesh.static_materials)
        for slot in slots:
            key=re.sub(r'[._]\d+$','',str(slot.get_editor_property('imported_material_slot_name')))
            assert key in mats,(row['name'],key)
            slot.material_interface=mats[key]
        mesh.static_materials=slots
        M.remove_collisions(mesh)
        mesh.set_editor_property('support_ray_tracing',False)
        assert A.save_loaded_asset(mesh,False),row['name']
        meshes[row['name']]=mesh
    settings={
        'moving_fixture_mesh':meshes['SM_NCLF_MovingOpticalHead'],
        'strobe_fixture_mesh':meshes['SM_NCLF_CompactStrobeBar'],
        'fixture_mount_mesh':meshes['SM_NCLF_StaticCeilingMount'],
        'beam_mesh':meshes['SM_NCLF_OpenBeamCone'],
        'beam_material':beam,'lens_material':lens,
        'moving_light_intensity':430.0,'strobe_light_intensity':65.0,
        'strobe_enabled':True,'club_effects_enabled':True,
        'club_exposure_bias':.25,'neon_bloom_intensity':.9,'neon_bloom_threshold':.65,
    }
    for key,value in settings.items():club.set_editor_property(key,value)
    club.refresh_club()
    assert all(club.get_editor_property(k)==v for k,v in protected_club.items()),'Music/room settings changed'
    lights=list(club.get_editor_property('club_lights'))
    fixtures=list(club.get_editor_property('club_fixtures'))
    mounts=list(club.get_editor_property('club_fixture_mounts'))
    beams=list(club.get_editor_property('club_beams'))
    assert len(lights)==6 and len(fixtures)==6 and len(mounts)==6 and len(beams)==4
    for c in fixtures+mounts+beams:
        assert str(c.get_collision_enabled())==str(unreal.CollisionEnabled.NO_COLLISION),c.get_name()
        assert not c.get_editor_property('cast_shadow'),c.get_name()
    for light in lights:
        assert not light.get_editor_property('cast_shadows'),light.get_name()
        assert light.indirect_lighting_intensity==0 and light.volumetric_scattering_intensity==0
    after={a.get_name():snapshot(a) for a in S.get_all_level_actors()}
    drift=[r['label'] for n,r in old.items() if r['label']!='NightClub_Ambience' and (n not in after or r!=after[n])]
    assert not drift,('Protected actor changes',drift)
    assert len(after)==len(old),'Unexpected map actor count change'
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    report=dict(status='PASS',actor_count=len(after),protected_actors=len(old)-1,changed_actor='NightClub_Ambience',
        music_room_door_pole_settings_unchanged=True,ordinary_lights_unchanged=True,
        real_lights=6,real_lights_per_room=3,moving_beams=4,fixtures=6,mounts=6,
        moving_intensity_cd=430,strobe_peak_cd=65,exposure_bias=.25,
        bloom=.9,shadowless=True,global_fog_added=False,
        settings={k:str(v) for k,v in settings.items()})
    (L/'lighting-install.json').write_text(json.dumps(report,indent=2))
    (L/'actors_after.json').write_text(json.dumps(list(after.values()),indent=2))
    unreal.log('NIGHTCLUB_LIGHTING_INSTALL_PASS protected='+str(report['protected_actors']))

try:
    run_lighting()
except:
    unreal.log_error('NIGHTCLUB_LIGHTING_INSTALL_FAIL '+traceback.format_exc())
