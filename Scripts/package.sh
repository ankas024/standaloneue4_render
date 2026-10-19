#!/bin/bash
# Builds the Render Studio executable and packages it into Dist/RenderStudio (Linux).
# Usage: Scripts/package.sh /path/to/UnrealEngine-4.27 [--skip-build] [--symbols]   (or set UE4_ROOT)
set -e
UE_ROOT="${1:-$UE4_ROOT}"
if [ -z "$UE_ROOT" ]; then
	echo "Pass the engine root, e.g. Scripts/package.sh ~/UnrealEngine-4.27"
	exit 1
fi
shift || true
PYTHON="$UE_ROOT/Engine/Binaries/ThirdParty/Python3/Linux/bin/python3"
[ -x "$PYTHON" ] || PYTHON=python3
exec "$PYTHON" "$(dirname "$0")/package.py" --engine "$UE_ROOT" "$@"
