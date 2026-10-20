#!/bin/bash
set -u
cd "/Users/Stewart/Documents/Unreal Projects/SeniorSendoff" || exit 1
echo "whoami=$(whoami) host=$(hostname) date=$(date) pwd=$PWD"
for E in "/Users/Shared/Epic Games/UE_5.8" "/Users/UE_5.8" "/Applications/UE_5.8" "$HOME/UnrealEngine/5.8"; do
  [ -x "$E/Engine/Build/BatchFiles/Mac/Build.sh" ] && ENGINE="$E" && break
done
echo "ENGINE=${ENGINE:-none}"
echo "--- xcode: $(xcode-select -p 2>&1)"; which clang python3 blender 2>&1
echo "--- unreal processes: $(pgrep -fl 'UnrealEditor' | grep -v Services | head -3)"
echo "--- running survey"
SCRIPT="$PWD/Tools/claude/dump_pigsty_and_closet.py"
"$ENGINE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/SeniorSendoff.uproject" -run=pythonscript -script="$SCRIPT" -unattended -nopause -nosplash -NullRHI -stdout -FullStdOutLogOutput 2>&1 | grep -a -i "CLAUDE_DUMP\|LogPython\|error\|Fatal\|Traceback\|Exception" | grep -v "LogShaderCompilers" | tail -80
echo "SURVEY_EXIT=${PIPESTATUS[0]} $(date)"
ls -la "$PWD/Saved/ClaudeDump/"
