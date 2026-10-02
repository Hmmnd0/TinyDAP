#!/bin/bash
# Converts audio files (mp3, m4a, flac, aiff, wav...) to 16-bit / 44.1 kHz PCM
# WAV for TinyDAP, keeping the folder structure. Uses macOS's afconvert.
#
# Usage: tools/to_wav.sh <source-folder> <dest-folder>
set -euo pipefail

src=${1:?usage: to_wav.sh <source-folder> <dest-folder>}
dst=${2:?usage: to_wav.sh <source-folder> <dest-folder>}

find "$src" -type f \( -iname '*.mp3' -o -iname '*.m4a' -o -iname '*.flac' -o -iname '*.aif' \
    -o -iname '*.aiff' -o -iname '*.wav' -o -iname '*.alac' \) -print0 |
while IFS= read -r -d '' f; do
    rel=${f#"$src"/}
    out="$dst/${rel%.*}.wav"
    mkdir -p "$(dirname "$out")"
    if [ -e "$out" ]; then
        continue
    fi
    echo "$rel"
    afconvert -f WAVE -d LEI16@44100 "$f" "$out"
done
