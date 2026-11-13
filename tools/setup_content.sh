#!/usr/bin/env bash
# Copy UE template packs (mannequins, input, prototyping) into Content/ (gitignored: they ship with the engine).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UE="${UE:-/media/itzzsuperrr/846e0635-f4f1-4d51-b053-9c416566a01f/UE/5.7.4}"
T="$UE/Templates/TemplateResources/High"
for p in Characters Input LevelPrototyping; do mkdir -p "$ROOT/Content/$p"; cp -rn "$T/$p/Content/." "$ROOT/Content/$p/"; done
echo ok
