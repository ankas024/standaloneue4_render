#!/usr/bin/env bash
# Development launch: runs Render Studio inside UE4Editor in application mode (level editor hidden).
# Usage: Scripts/renderstudio.sh /path/to/UnrealEngine-4.27   (or export UE4_ROOT)
set -euo pipefail
UE_ROOT="${1:-${UE4_ROOT:-}}"
if [ -z "$UE_ROOT" ]; then
	echo "Pass the engine root, e.g. Scripts/renderstudio.sh ~/UnrealEngine-4.27" >&2
	exit 1
fi
PROJECT="$(cd "$(dirname "$0")/.." && pwd)/RenderStudio.uproject"
exec "$UE_ROOT/Engine/Binaries/Linux/UE4Editor" "$PROJECT" -RenderStudio
