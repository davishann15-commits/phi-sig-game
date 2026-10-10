import pathlib, unreal
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NightClub'
exec((B/'lighting_layout/install_lighting.py').read_text().split('def run_lighting():')[0],globals())
authored_material('M_NCLF_Beam',True)
unreal.log('NIGHTCLUB_BEAM_POLISH_PASS')
