#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
engine_dir="${SSO_UNREAL_ENGINE:-$HOME/UnrealEngine/5.8.2}"

if [[ "$(uname -s)" != "Linux" ]]; then
  printf 'Run this helper on the receiving Linux development PC.\n' >&2
  exit 1
fi

build_script="$engine_dir/Engine/Build/BatchFiles/Linux/Build.sh"
editor="$engine_dir/Engine/Binaries/Linux/UnrealEditor"
if [[ ! -f "$build_script" || ! -x "$editor" ]]; then
  printf 'Set SSO_UNREAL_ENGINE to a working Unreal 5.8.2 engine root (the folder containing Engine).\nChecked: %s\n' "$engine_dir" >&2
  exit 1
fi

cd -- "$project_dir"
if [[ -e .git ]]; then
  git lfs version >/dev/null
  git lfs install --local
  git lfs pull
fi

python3 - "$engine_dir" "$project_dir" <<'PY'
import json
import sys
from pathlib import Path

engine = Path(sys.argv[1])
project = Path(sys.argv[2])
version = json.loads((engine / 'Engine/Build/Build.version').read_text())
numbers = tuple(version[key] for key in ('MajorVersion', 'MinorVersion', 'PatchVersion'))
if numbers != (5, 8, 2):
    raise SystemExit(f'This snapshot was saved with Unreal 5.8.2; found {numbers}. Set SSO_UNREAL_ENGINE to 5.8.2.')

missing = []
for path in (project / 'Content').rglob('*'):
    if path.suffix.lower() not in {'.uasset', '.umap', '.ubulk', '.uexp', '.uptnl'}:
        continue
    with path.open('rb') as file:
        if file.read(128).startswith(b'version https://git-lfs.github.com/spec/v1'):
            missing.append(str(path.relative_to(project)))
if missing:
    raise SystemExit('Git LFS asset downloads are incomplete. Run git lfs pull.\n' + '\n'.join(missing[:12]))
if not (project / 'Content/Story/Maps/Chapter01_House.umap').is_file():
    raise SystemExit('The Chapter01_House map is missing from this copy.')
print('Unreal 5.8.2 and downloaded game assets verified.')
PY

export DOTNET_PROCESSOR_COUNT="${SSO_BUILD_PROCESSORS:-${DOTNET_PROCESSOR_COUNT:-4}}"
bash "$build_script" SeniorSendoffEditor Linux Development \
  "-Project=$project_dir/SeniorSendoff.uproject" -NoHotReload \
  "-MaxParallelActions=${SSO_BUILD_PROCESSORS:-4}"

printf '\nEditor build complete. Open this project with Unreal 5.8.2:\n%s\n' "$project_dir/SeniorSendoff.uproject"
