#!/usr/bin/env bash
# Builds, cooks and packages Last Block for iPhone (Shipping, IoStore).
# Needs: a Mac with Xcode, Unreal Engine 5.8, and signing set up in
# Project Settings -> Platforms -> iOS (team, bundle id, provisioning).
#
#   UE_ROOT="/Users/Shared/Epic Games/UE_5.8" ./Scripts/build-ios.sh
#   CONFIG=Development ./Scripts/build-ios.sh      # with logs and the console
set -euo pipefail

UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
CONFIG="${CONFIG:-Shipping}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT="$HERE/LastBlock.uproject"
OUT="$HERE/Build/IOS-$CONFIG"

if [ ! -x "$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" ]; then
  echo "Unreal Engine not found at: $UE_ROOT (set UE_ROOT)" >&2
  exit 1
fi

"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
  -project="$PROJECT" \
  -platform=IOS \
  -clientconfig="$CONFIG" \
  -build -cook -stage -package -pak -iostore -compressed \
  -archive -archivedirectory="$OUT" \
  -utf8output -nop4 -unattended

echo
echo "Done: $OUT"
echo "Install the .ipa with Xcode (Window -> Devices and Simulators) or Apple Configurator."
