#!/usr/bin/env bash
# Package a Linux build (pipeline check; the jam build is Windows: tools/package_win.bat). Output: Build/Linux
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE="${UE:-/media/itzzsuperrr/846e0635-f4f1-4d51-b053-9c416566a01f/UE/5.7.4}"
"$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$ROOT/ClassroomLevelling.uproject" -noP4 -platform=Linux \
  -clientconfig=Shipping -build -cook -stage -pak -iostore -prereqs -archive -archivedirectory="$ROOT/Build" -nodebuginfo -utf8output
# Piper (runtime voices) lives outside Content: copy it next to the packaged project dir.
D="$ROOT/Build/Linux/ClassroomLevelling/ThirdParty/Piper"; mkdir -p "$D"
cp -r "$ROOT/ThirdParty/Piper/piper" "$ROOT/ThirdParty/Piper/voice.onnx" "$ROOT/ThirdParty/Piper/voice.onnx.json" "$D/"
du -sh "$ROOT/Build/Linux"
