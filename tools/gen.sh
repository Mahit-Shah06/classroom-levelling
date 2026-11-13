#!/usr/bin/env bash
# Offload bulk content writing to Cursor CLI (Grok/auto, free tier). Usage: tools/gen.sh <out.json> "<request>"
set -euo pipefail
OUT="$1"; REQ="$2"; ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SCHEMA="$(sed -n '/USTRUCT/,$p' "$ROOT/Source/ClassroomLevelling/Narrative/NarrativeTypes.h")"
CTX="$(cat "$ROOT/docs/DESIGN.md")"
TMP=$(mktemp -d); cd "$TMP"
~/.local/bin/agent -p --trust --model "${MODEL:-auto}" "You write game content as JSON only. Output ONLY a JSON array, no prose, no code fences.
Field names must match these UE structs exactly (Sub-stat keys look like \"Intellect.Logic\"; tags are lowercase words such as obedience, empathy, ambition, cunning, betrayal, chaos, withdrawal, loyalty, honesty, cruelty, humor; NPC refs: never hardcode NPC ids in storylets; use @rival @ally @gremlin @president @ex1 @broker and staff @head @homeroom @proctor @nurse @coach @counsellor (rolled per run) or @slotname, and {rival}/{slotname} in Text; NO names from existing anime/games (e.g. Classroom of the Elite); conditions use: flag:x, !flag:x, stat:Key>=N, rel:npc>N, rel:@slot>N, tag:x>=N, uniq>N):
$SCHEMA
Game design context:
$CTX
REQUEST: $REQ" > "$TMP/raw.txt"
python3 -I -c 'import json,sys,re; t=open(sys.argv[1]).read(); t=t[t.find("["):t.rfind("]")+1]; json.dump(json.loads(t),open(sys.argv[2],"w"),indent=1,ensure_ascii=False)' "$TMP/raw.txt" "$ROOT/$OUT"
echo "wrote $OUT"; rm -rf "$TMP"
