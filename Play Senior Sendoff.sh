#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
package="$project_dir/Deliverables/CombinedLinux/Linux/Play Senior Sendoff.sh"
if [[ "${1:-}" != "--editor" && -x "$package" ]]; then
  exec "$package" "$@"
fi
if [[ "${1:-}" == "--editor" ]]; then shift; fi
engine_dir="${SSO_UNREAL_ENGINE:-$HOME/UnrealEngine/5.8.2}"
editor="$engine_dir/Engine/Binaries/Linux/UnrealEditor"
if [[ ! -x "$editor" ]]; then
  printf 'Unreal Editor was not found at %s\n' "$editor" >&2
  exit 1
fi

exec "$editor" "$project_dir/SeniorSendoff.uproject" /Game/Story/Maps/Lobby \
  -game -windowed -ResX=1600 -ResY=900 -nosplash -NoTraceServer "$@"
