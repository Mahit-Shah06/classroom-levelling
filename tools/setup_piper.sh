#!/usr/bin/env bash
# Fetch standalone piper (C++ release) + libritts_r voice into ThirdParty/Piper (gitignored). Usage: tools/setup_piper.sh [linux|windows]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; D="$ROOT/ThirdParty/Piper"; mkdir -p "$D"; cd "$D"
REL=https://github.com/rhasspy/piper/releases/download/2023.11.14-2
case "${1:-linux}" in
  linux) curl -sSL "$REL/piper_linux_x86_64.tar.gz" | tar xz ;;
  windows) curl -sSL -o w.zip "$REL/piper_windows_amd64.zip" && unzip -oq w.zip && rm w.zip ;;
esac
M=https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/libritts_r/medium/en_US-libritts_r-medium.onnx
[ -f voice.onnx ] || curl -sSL -o voice.onnx "$M"; [ -f voice.onnx.json ] || curl -sSL -o voice.onnx.json "$M.json"
echo ok
