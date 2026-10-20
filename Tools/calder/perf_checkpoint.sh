#!/bin/bash
# Current appearance/terrain checkpoint: original 300-frame/4-second method,
# three alternating rounds, including the five Tier A interior comparison eyes.
set -eu
project='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff'
runner="$project/Saved/CalderWork/tools/run_game.sh"
tag=${1:?Usage: perf_checkpoint.sh tag comparison-switch}
comparison=${2:?Comparison switch required}
focus=${3:-all}
views='front_street,street_along,detail_establishing_corner,detail_street_dusk,detail_clocktower_far,detail_campus_quad,detail_layout_quad,detail_layout_gateway,detail_stadium_gate,detail_layout_aerial,detail_layout_dormrow,detail_perf_in_'
if [ "$focus" = interiors ]; then views='detail_perf_in_'; fi
for round in 1 2 3; do
    "$runner" "${tag}_on${round}" 900 -RenderOffScreen -MetalOffscreenOnly -nosound -HouseExteriorViews -CleanViews -ExteriorViewSamples=300 -ExteriorViewWarmup=4 -CalderSceneTime=120 "-ExteriorViewFilter=$views" '-ExecCmds=t.MaxFPS 0,r.InstanceCulling.OcclusionCull 1' > /dev/null
    "$runner" "${tag}_off${round}" 900 -RenderOffScreen -MetalOffscreenOnly -nosound -HouseExteriorViews -CleanViews -ExteriorViewSamples=300 -ExteriorViewWarmup=4 -CalderSceneTime=120 "-ExteriorViewFilter=$views" '-ExecCmds=t.MaxFPS 0,r.InstanceCulling.OcclusionCull 1' "$comparison" > /dev/null
done
if [ "$focus" = interiors ]; then
    python3 "$project/Tools/calder/report_checkpoint_perf.py" "$tag" --interiors
else
    python3 "$project/Tools/calder/report_checkpoint_perf.py" "$tag"
fi
