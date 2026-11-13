#!/usr/bin/env bash
# Import Assets/Generated/*.glb into Content/Generated via the editor's Python (opens the editor briefly, quits when done).
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE="${UE:-/media/itzzsuperrr/846e0635-f4f1-4d51-b053-9c416566a01f/UE/5.7.4}"
"$UE/Engine/Binaries/Linux/UnrealEditor" "$ROOT/ClassroomLevelling.uproject" -ExecutePythonScript="$ROOT/tools/import_assets.py" -unattended -nosplash -log >/dev/null 2>&1
grep -E "\[import\]|Assertion|Python: Traceback|Error:.*py" "$ROOT/Saved/Logs/ClassroomLevelling.log" | cut -c1-200
