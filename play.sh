#!/usr/bin/env bash
# Launch the game standalone (windowed). First run may compile shaders.
UE=/media/itzzsuperrr/846e0635-f4f1-4d51-b053-9c416566a01f/UE/5.7.4
exec "$UE/Engine/Binaries/Linux/UnrealEditor" "$(dirname "$(readlink -f "$0")")/ClassroomLevelling.uproject" -game -windowed -ResX=1600 -ResY=900 -log "$@"
