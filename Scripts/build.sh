#!/usr/bin/env bash
# Builds the Render Studio editor target against a UE 4.27 source build (Linux).
# Usage: Scripts/build.sh /path/to/UnrealEngine-4.27   (or export UE4_ROOT)
set -euo pipefail
UE_ROOT="${1:-${UE4_ROOT:-}}"
if [ -z "$UE_ROOT" ]; then
	echo "Pass the engine root, e.g. Scripts/build.sh ~/UnrealEngine-4.27" >&2
	exit 1
fi
PROJECT="$(cd "$(dirname "$0")/.." && pwd)/RenderStudio.uproject"
"$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" RenderStudioEditor Linux Development -Project="$PROJECT" -WaitMutex
