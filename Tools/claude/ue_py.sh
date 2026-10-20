#!/usr/bin/env bash
# Run an editor Python script headless. Usage: bash ue_py.sh <script.py> [logname]
set -uo pipefail
PROJECT="/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SeniorSendoff.uproject"
UE="/Users/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd"
SCRIPT="$1"; NAME="${2:-$(basename "$SCRIPT" .py)}"
LOGDIR="/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/Saved/ClaudeDump"
mkdir -p "$LOGDIR"
echo "START $(date)" > "$LOGDIR/$NAME.status"
"$UE" "$PROJECT" -run=pythonscript -script="$SCRIPT" -unattended -nosplash -nopause -NoTraceServer -stdout -FullStdOutLogOutput > "$LOGDIR/$NAME.log" 2>&1
echo "EXIT $? $(date)" >> "$LOGDIR/$NAME.status"
tail -n 30 "$LOGDIR/$NAME.log"
