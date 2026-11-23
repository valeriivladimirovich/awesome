#!/usr/bin/env bash
# Compiles the C++ module for the Mac editor, then you can open LastBlock.uproject.
set -euo pipefail
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
"$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" LastBlockEditor Mac Development -Project="$HERE/LastBlock.uproject" -WaitMutex
