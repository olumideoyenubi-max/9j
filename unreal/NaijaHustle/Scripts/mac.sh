#!/bin/zsh
# Build and run NAIJA HUSTLE on a Mac without opening Xcode.
#
#   Scripts/mac.sh build              compile the editor target
#   Scripts/mac.sh editor             open the project in the Unreal Editor
#   Scripts/mac.sh play               run the street level as a standalone game (1280x720 window)
#   Scripts/mac.sh script <file.py>   run an editor Python script, then quit
#
# By default it uses the project this script sits in and the engine at
# "/Users/Shared/Epic Games/UE_5.8". Override either one:
#   UE_ROOT="/path/to/UE_5.8" NH_PROJECT="/path/to/Your.uproject" Scripts/mac.sh play
set -e
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HERE="${0:A:h}"
NH_PROJECT="${NH_PROJECT:-$(ls "$HERE"/../*.uproject | head -1)}"
NAME="${${NH_PROJECT:t}%.uproject}"
EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
MAP="/Game/NaijaHustle/Maps/L_Slice_Street"
[[ -x "$EDITOR" ]] || { echo "Unreal Editor not found under $UE_ROOT. Set UE_ROOT."; exit 1; }

case "$1" in
  build)  "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" "${NAME}Editor" Mac Development -project="$NH_PROJECT" ;;
  editor) "$EDITOR" "$NH_PROJECT" ;;
  # caffeinate keeps the Mac awake while the game runs
  play)   caffeinate -dims "$EDITOR" "$NH_PROJECT" "$MAP" -game -windowed -ResX=1280 -ResY=720 -log "${@:2}" ;;
  script) "$EDITOR" "$NH_PROJECT" -ExecutePythonScript="$2" ;;
  *)      sed -n '2,11p' "$0" | sed 's/^# \{0,1\}//' ;;
esac
