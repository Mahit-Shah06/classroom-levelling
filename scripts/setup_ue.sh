#!/usr/bin/env bash
# Usage: scripts/setup_ue.sh /path/to/Linux_Unreal_Engine_X.zip
set -euo pipefail
ZIP="${1:-$(ls -t ~/Downloads/Linux_Unreal_Engine_*.zip 2>/dev/null | head -1)}"
DEST=/media/itzzsuperrr/846e0635-f4f1-4d51-b053-9c416566a01f/UE
[ -f "$ZIP" ] || { echo "Zip not found. Pass its path."; exit 1; }
sudo apt install -y mesa-vulkan-drivers vulkan-tools unzip libnss3 libxcomposite1 libxcursor1 libxdamage1 libxrandr2 libxkbcommon0
vulkaninfo --summary | grep -i deviceName || echo "WARNING: no Vulkan device"
mkdir -p "$DEST"
unzip -q -n "$ZIP" -d "$DEST"
EDITOR=$(find "$DEST" -maxdepth 6 -name UnrealEditor -type f | head -1)
echo "Editor: ${EDITOR:-NOT FOUND}"
