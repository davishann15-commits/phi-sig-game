import pathlib,unreal
B=pathlib.Path(unreal.Paths.project_dir())/'SourceArt/NoodleWing'
exec((B/'import_art.py').read_text(),globals())
exec((B/'install.py').read_text(),globals())
