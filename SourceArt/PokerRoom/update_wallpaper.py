import json,os,unreal
ROOT='/Users/Stewart/Projects/Senior sendoff/outputs/poker-room-v1'
helper=os.path.join(unreal.Paths.project_content_dir(),'Python/apply_bedroom.py')
exec(compile(open(helper).read().split('\ntextures={}')[0].replace("if name.startswith('SM'):","if True:"),helper,'exec'))
DEST='/Game/SeniorSendOff/House/PokerRoom'
m=imp(ROOT+'/geometry/POK_SM_Wallpaper.fbx','POK_SM_Wallpaper')
m.set_material(0,A.load_asset(DEST+'/POK_M_WallWine'));A.save_loaded_asset(m)
unreal.log('POKER_WALLPAPER_POLISH_PASSED')
