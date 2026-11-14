#!/usr/bin/env bash
# Import generated models via the editor's Python (opens the editor briefly, quits when done).
#   tools/import_assets.sh          props:      Assets/Generated/*.fbx  -> Content/Props/<id>
#   tools/import_assets.sh chars    characters: Assets/Characters/*.fbx -> Content/Chars/<id>
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE="${UE:-/media/itzzsuperrr/846e0635-f4f1-4d51-b053-9c416566a01f/UE/5.7.4}"
PY="import_assets.py"; [ "${1:-}" = chars ] && PY="import_chars.py"
"$UE/Engine/Binaries/Linux/UnrealEditor" "$ROOT/ClassroomLevelling.uproject" -ExecutePythonScript="$ROOT/tools/$PY" -unattended -nosplash -log >/dev/null 2>&1
grep -E "\[import\]|Assertion|Python: Traceback|Error:.*py|Message dialog" "$ROOT/Saved/Logs/ClassroomLevelling.log" | cut -c1-200
