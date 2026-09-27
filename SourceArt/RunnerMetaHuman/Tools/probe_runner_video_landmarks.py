import unreal

path = '/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/RunnerVideoHeadReview/TargetPortrait.png'
size, pixels = unreal.PromotedFrameUtils.get_promoted_frame_as_pixel_array_from_disk(path)
editor = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
result = editor.track_face_landmarks_from_image(pixels, size.x, size.y)
if isinstance(result, tuple) and len(result) == 1:
    result = result[0]
unreal.log('RUNNER_VIDEO_LANDMARKS ' + str(size) + ' curves=' + str(len(result) if result else 0))
if result:
    unreal.log('RUNNER_VIDEO_LANDMARK_NAMES ' + ','.join(sorted(result.keys())[:30]))
