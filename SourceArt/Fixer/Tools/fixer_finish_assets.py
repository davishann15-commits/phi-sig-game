"""Normalize assembled names, export Blender inputs, and style Fixer only."""
import unreal, json, runpy, traceback
from pathlib import Path
A=unreal.EditorAssetLibrary
ROOT='/Game/MetaHumans/Fixer/MH_Fixer'
report={}
try:
    # Renaming the preserved source study changed some baked subobject names.
    # Normalize those freshly-created outputs without touching other characters.
    for sub,old,new in [
        ('Body','SKM_MH_Fixer_SkinStudyBackup_BodyMesh','SKM_MH_Fixer_BodyMesh'),
        ('Face','SKM_MH_Fixer_SkinStudyBackup_FaceMesh','SKM_MH_Fixer_FaceMesh'),
        ('Clothing','MH_Fixer_SkinStudyBackup_Outfits','MH_Fixer_Outfits')]:
        src=ROOT+'/'+sub+'/'+old;dst=ROOT+'/'+sub+'/'+new
        if not A.does_asset_exist(dst):
            assert A.does_asset_exist(src),src
            assert A.rename_asset(src,dst),dst
    for name,path in [('FixerNativeBody',ROOT+'/Body/SKM_MH_Fixer_BodyMesh'),('FixerNativeOutfit',ROOT+'/Clothing/MH_Fixer_Outfits')]:
        obj=A.load_asset(path);assert obj,path
        if name=='FixerNativeOutfit':
            cp=ROOT+'/Details/Hoodie/SK_ExportSource'
            obj=A.load_asset(cp) if A.does_asset_exist(cp) else A.duplicate_asset(path,cp)
            assert unreal.SeniorCharacterAssetTools.prepare_braxton_hoodie_export(obj)
            A.save_loaded_asset(obj)
        task=unreal.AssetExportTask();task.object=obj;task.filename='/private/tmp/'+name+'.fbx'
        task.automated=True;task.prompt=False;task.replace_identical=True;task.options=unreal.FbxExportOption()
        task.options.level_of_detail=False;task.options.export_morph_targets=False
        assert unreal.Exporter.run_asset_export_task(task),name
        report[name]=task.filename
    A.save_directory(ROOT,only_if_is_dirty=True,recursive=True)
    runpy.run_path('/private/tmp/fixer_detail_materials.py')
    runpy.run_path('/private/tmp/fixer_retarget.py')
    report['complete']=True
    unreal.log('FIXER_FINISH_ASSETS_COMPLETE')
except Exception:
    report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:Path('/private/tmp/fixer_finish_report.json').write_text(json.dumps(report,indent=2))
