#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
engine_dir="${SSO_UNREAL_ENGINE:-$HOME/UnrealEngine/5.8.2}"
archive_dir="${SSO_PACKAGE_DIR:-$project_dir/Deliverables/CombinedLinux}"
"$engine_dir/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
  "-project=$project_dir/SeniorSendoff.uproject" -noP4 -platform=Linux \
  -clientconfig=Development -build -cook -iterativecooking -stage -package -pak -archive \
  "-archivedirectory=$archive_dir" -unattended -utf8output

game="$archive_dir/Linux/SeniorSendoff/Binaries/Linux/SeniorSendoff"
test -f "$game"
cat > "$archive_dir/Linux/Play Senior Sendoff.sh" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
game="$root/SeniorSendoff/Binaries/Linux/SeniorSendoff"
if [[ ! -x "$game" ]]; then chmod +x "$game"; fi
exec "$game" SeniorSendoff "$@"
SH
chmod +x "$archive_dir/Linux/Play Senior Sendoff.sh"
printf 'Runnable game: %s\n' "$archive_dir/Linux/Play Senior Sendoff.sh"
