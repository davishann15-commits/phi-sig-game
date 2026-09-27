import unreal
root='/Game/MetaHumans/RunnerRebuild/MH_Runner_Working'
for label,path in [('body',root+'/Body/SKM_MH_Runner_Working_BodyMesh'),('outfit',root+'/Details/RunnerOutfit/SK_Runner_MetaOutfit'),('native',root+'/Details/RunnerOutfit/SK_Runner_NativeOutfit'),('default',root+'/Clothing/MH_Runner_Working_Outfits')]:
    mesh=unreal.load_asset(path)
    unreal.log('RUNNER_MESH_BOUNDS '+label+' '+str(mesh.get_bounds())+' '+str(mesh.get_editor_property('skeleton').get_path_name()))
